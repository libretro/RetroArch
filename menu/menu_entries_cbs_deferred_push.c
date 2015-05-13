/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <file/file_path.h>
#include "menu.h"
#include "menu_displaylist.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"

#include "../file_ext.h"
#include "../retroarch.h"
#include "../settings.h"
#include "../performance.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "menu_database.h"

#include "../gfx/video_shader_driver.h"
#include "../git_version.h"
#include "../config.features.h"

#ifdef HAVE_LIBRETRODB
static int create_string_list_rdb_entry_string(const char *desc, const char *label,
      const char *actual_string, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   str_len += strlen(actual_string) + 1;
   string_list_append(str_list, actual_string, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %s", desc, actual_string);
   menu_list_push(list, tmp, output_label, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int create_string_list_rdb_entry_int(const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char str[PATH_MAX_LENGTH];
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   snprintf(str, sizeof(str), "%d", actual_int);
   str_len += strlen(str) + 1;
   string_list_append(str_list, str, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %d", desc, actual_int);
   menu_list_push(list, tmp, output_label,
         0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}
#endif

static int deferred_push_core_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORE_INFO);
}

static int deferred_push_system_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   {
      char tmp[PATH_MAX_LENGTH];
      char tmp2[PATH_MAX_LENGTH];
      const char *tmp_string = NULL;
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();

      snprintf(tmp, sizeof(tmp), "Build date: %s", __DATE__);
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      (void)tmp_string;

#ifdef HAVE_GIT_VERSION
      snprintf(tmp, sizeof(tmp), "Git version: %s", rarch_git_version);
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
#endif

      rarch_print_compiler(tmp, sizeof(tmp));
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      {
         char cpu_str[PATH_MAX_LENGTH];
         uint64_t cpu = rarch_get_cpu_features();

         snprintf(cpu_str, sizeof(cpu_str), "CPU Features: ");

         if (cpu & RETRO_SIMD_MMX)
            strlcat(cpu_str, "MMX ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_MMXEXT)
            strlcat(cpu_str, "MMXEXT ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSE)
            strlcat(cpu_str, "SSE1 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSE2)
            strlcat(cpu_str, "SSE2 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSE3)
            strlcat(cpu_str, "SSE3 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSSE3)
            strlcat(cpu_str, "SSSE3 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSE4)
            strlcat(cpu_str, "SSE4 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_SSE42)
            strlcat(cpu_str, "SSE4.2 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_AVX)
            strlcat(cpu_str, "AVX ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_AVX2)
            strlcat(cpu_str, "AVX2 ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_VFPU)
            strlcat(cpu_str, "VFPU ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_NEON)
            strlcat(cpu_str, "NEON ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_PS)
            strlcat(cpu_str, "PS ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_AES)
            strlcat(cpu_str, "AES ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_VMX)
            strlcat(cpu_str, "VMX ", sizeof(cpu_str));
         if (cpu & RETRO_SIMD_VMX128)
            strlcat(cpu_str, "VMX128 ", sizeof(cpu_str));
         menu_list_push(list, cpu_str, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (frontend)
      {
         int major = 0, minor = 0;

         snprintf(tmp, sizeof(tmp), "Frontend identifier: %s",
               frontend->ident);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         if (frontend->get_name)
         {
            frontend->get_name(tmp2, sizeof(tmp2));
            snprintf(tmp, sizeof(tmp), "Frontend name: %s",
                  frontend->get_name ? tmp2 : "N/A");
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (frontend->get_os)
         {
            frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
            snprintf(tmp, sizeof(tmp), "Frontend OS: %s %d.%d",
                  frontend->get_os ? tmp2 : "N/A", major, minor);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         snprintf(tmp, sizeof(tmp), "RetroRating level: %d", 
               frontend->get_rating ? frontend->get_rating() : -1);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         if (frontend->get_powerstate)
         {
            int seconds = 0, percent = 0;
            enum frontend_powerstate state = frontend->get_powerstate(&seconds, &percent);

            tmp2[0] = '\0';

            if (percent != 0)
               snprintf(tmp2, sizeof(tmp2), "%d%%", percent);

            switch (state)
            {
               case FRONTEND_POWERSTATE_NONE:
                  strlcat(tmp2, " N/A", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_NO_SOURCE:
                  strlcat(tmp2, " (No source)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_CHARGING:
                  strlcat(tmp2, " (Charging)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_CHARGED:
                  strlcat(tmp2, " (Charged)", sizeof(tmp));
                  break;
               case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
                  strlcat(tmp2, " (Discharging)", sizeof(tmp));
                  break;
            }

            snprintf(tmp, sizeof(tmp), "Power source : %s",
                  tmp2);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }

#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
      tmp_string = gfx_ctx_get_ident();

      snprintf(tmp, sizeof(tmp), "Video context driver: %s",
            tmp_string ? tmp_string : "N/A");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      {
         float val = 0.0f;
         if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_WIDTH, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric width (mm): %.2f", 
                  val);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_HEIGHT, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric height (mm): %.2f", 
                  val);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }

         if (gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &val))
         {
            snprintf(tmp, sizeof(tmp), "Display metric DPI: %.2f", 
                  val);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
#endif

   }

   {
      char feat_str[PATH_MAX_LENGTH];

      snprintf(feat_str, sizeof(feat_str),
            "LibretroDB support: %s", _libretrodb_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Overlay support: %s", _overlay_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Command interface support: %s", _command_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Network Command interface support: %s", _network_command_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Cocoa support: %s", _cocoa_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "PNG support (RPNG): %s", _rpng_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
      
      snprintf(feat_str, sizeof(feat_str),
            "SDL1.2 support: %s", _sdl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "SDL2 support: %s", _sdl2_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);


      snprintf(feat_str, sizeof(feat_str),
            "OpenGL support: %s", _opengl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenGL ES support: %s", _opengles_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Threading support: %s", _thread_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "KMS/EGL support: %s", _kms_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Udev support: %s", _udev_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenVG support: %s", _vg_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "EGL support: %s", _egl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "X11 support: %s", _x11_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Wayland support: %s", _wayland_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "XVideo support: %s", _xvideo_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "ALSA support: %s", _alsa_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OSS support: %s", _oss_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenAL support: %s", _al_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenSL support: %s", _sl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "RSound support: %s", _rsound_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "RoarAudio support: %s", _roar_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "JACK support: %s", _jack_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "PulseAudio support: %s", _pulse_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "DirectSound support: %s", _dsound_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "XAudio2 support: %s", _xaudio_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Zlib support: %s", _zlib_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "7zip support: %s", _7zip_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Dynamic library support: %s", _dylib_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Cg support: %s", _cg_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "GLSL support: %s", _glsl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "HLSL support: %s", _hlsl_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "libxml2 XML parsing support: %s", _libxml2_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
      
      snprintf(feat_str, sizeof(feat_str),
            "SDL image support: %s", _sdl_image_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "OpenGL/Direct3D render-to-texture (multi-pass shaders) support: %s", _fbo_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "FFmpeg support: %s", _ffmpeg_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "CoreText support: %s", _coretext_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "FreeType support: %s", _freetype_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Netplay (peer-to-peer) support: %s", _netplay_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Python (script support in shaders) support: %s", _python_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Video4Linux2 support: %s", _v4l2_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(feat_str, sizeof(feat_str),
            "Libusb support: %s", _libusb_supp ? "true" : "false");
      menu_list_push(list, feat_str, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);
   }

   menu_driver_populate_entries(path, label, type);

   return 0;
}

static int deferred_push_rdb_entry_detail(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int ret = 0;
#ifdef HAVE_LIBRETRODB
   content_playlist_t *playlist;
   char query[PATH_MAX_LENGTH];
   char path_rdl[PATH_MAX_LENGTH], path_base[PATH_MAX_LENGTH];
   unsigned i, j;
   database_info_list_t *db_info = NULL;
   file_list_t *list             = NULL;
   file_list_t *menu_list        = NULL;
   struct string_list *str_list  = NULL;
   menu_handle_t *menu           = menu_driver_get_ptr();
   settings_t *settings          = config_get_ptr();
   if (!menu)
      return -1;

   list             = (file_list_t*)data;
   menu_list        = (file_list_t*)userdata;
   str_list         = string_split(label, "|"); 

   if (!str_list)
      return -1;

   if (!list || !menu_list)
   {
      ret = -1;
      goto done;
   }

   strlcpy(query, "{'name':\"", sizeof(query));
   strlcat(query, str_list->elems[1].data, sizeof(query));
   strlcat(query, "\"}", sizeof(query));

   menu_list_clear(list);

   if (!(db_info = database_info_list_new(path, query)))
   {
      ret = -1;
      goto done;
   }

   strlcpy(path_base, path_basename(path), sizeof(path_base));
   path_remove_extension(path_base);
   strlcat(path_base, ".rdl", sizeof(path_base));

   fill_pathname_join(path_rdl, settings->content_database, path_base,
         sizeof(path_rdl));

   menu_database_realloc(path_rdl, false);

   playlist = menu->db_playlist;

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH];
      database_info_t *db_info_entry = &db_info->list[i];

      if (!db_info_entry)
         continue;


      if (db_info_entry->name)
      {
         snprintf(tmp, sizeof(tmp), "Name: %s", db_info_entry->name);
         menu_list_push(list, tmp, "rdb_entry_name",
               0, 0);
      }
      if (db_info_entry->description)
      {
         snprintf(tmp, sizeof(tmp), "Description: %s", db_info_entry->description);
         menu_list_push(list, tmp, "rdb_entry_description",
               0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string("Publisher", "rdb_entry_publisher",
               db_info_entry->publisher, path, list) == -1)
            return -1;
      }
      if (db_info_entry->developer)
      {
         if (create_string_list_rdb_entry_string("Developer", "rdb_entry_developer",
               db_info_entry->developer, path, list) == -1)
            return -1;
      }
      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string("Origin", "rdb_entry_origin",
               db_info_entry->origin, path, list) == -1)
            return -1;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string("Franchise", "rdb_entry_franchise",
               db_info_entry->franchise, path, list) == -1)
            return -1;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int("Max Users",
               "rdb_entry_max_users", db_info_entry->max_users,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Famitsu Magazine Rating",
               "rdb_entry_famitsu_magazine_rating", db_info_entry->famitsu_magazine_rating,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_review)
      {
         if (create_string_list_rdb_entry_string("Edge Magazine Review", "rdb_entry_edge_magazine_review",
               db_info_entry->edge_magazine_review, path, list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Rating",
               "rdb_entry_edge_magazine_rating", db_info_entry->edge_magazine_rating,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Issue",
               "rdb_entry_edge_magazine_issue", db_info_entry->edge_magazine_issue,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int("Releasedate Month",
               "rdb_entry_releasemonth", db_info_entry->releasemonth,
               path, list) == -1)
            return -1;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int("Releasedate Year",
               "rdb_entry_releaseyear", db_info_entry->releaseyear,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string("BBFC Rating", "rdb_entry_bbfc_rating",
               db_info_entry->bbfc_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string("ESRB Rating", "rdb_entry_esrb_rating",
               db_info_entry->esrb_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string("ELSPA Rating", "rdb_entry_elspa_rating",
               db_info_entry->elspa_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string("PEGI Rating", "rdb_entry_pegi_rating",
               db_info_entry->pegi_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string("Enhancement Hardware", "rdb_entry_enhancement_hw",
               db_info_entry->enhancement_hw, path, list) == -1)
            return -1;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string("CERO Rating", "rdb_entry_cero_rating",
               db_info_entry->cero_rating, path, list) == -1)
            return -1;
      }
      snprintf(tmp, sizeof(tmp),
            "Analog supported: %s",
            (db_info_entry->analog_supported == 1)  ? "true" : 
            (db_info_entry->analog_supported == -1) ? "N/A"  : "false");
      menu_list_push(list, tmp, "rdb_entry_analog",
            0, 0);
      snprintf(tmp, sizeof(tmp),
            "Rumble supported: %s",
            (db_info_entry->rumble_supported == 1)  ? "true" : 
            (db_info_entry->rumble_supported == -1) ? "N/A"  :  "false");
      menu_list_push(list, tmp, "rdb_entry_rumble",
            0, 0);

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string("CRC32 Checksum",
               "rdb_entry_crc32", db_info_entry->crc32,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string("SHA1 Checksum",
               "rdb_entry_sha1", db_info_entry->sha1,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string("MD5 Checksum",
               "rdb_entry_md5", db_info_entry->md5,
               path, list) == -1)
            return -1;
      }

      if (playlist)
      {
         for (j = 0; j < playlist->size; j++)
         {
            char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
            bool match_found = false;
            struct string_list *tmp_str_list = string_split(
                  playlist->entries[j].core_name, "|"); 

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
               strlcpy(elem0, tmp_str_list->elems[0].data, sizeof(elem0));
            if (tmp_str_list->size > 1)
               strlcpy(elem1, tmp_str_list->elems[1].data, sizeof(elem1));

            if (!strcmp(elem1, "crc"))
            {
               if (!strcmp(db_info_entry->crc32, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "sha1"))
            {
               if (!strcmp(db_info_entry->sha1, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "md5"))
            {
               if (!strcmp(db_info_entry->md5, elem0))
                  match_found = true;
            }

            string_list_free(tmp_str_list);

            if (!match_found)
               continue;

            rdb_entry_start_game_selection_ptr = j;
            menu_list_push(list, "Start Content", "rdb_entry_start_game",
                  MENU_FILE_PLAYLIST_ENTRY, 0);
         }
      }
   }

   if (db_info->count < 1)
      menu_list_push(list,
            "No information available.", "",
            0, 0);

   menu_driver_populate_entries(path, str_list->elems[0].data, type);

   ret = 0;

done:
   string_list_free(str_list);
#endif

   return ret;
}

static int deferred_push_core_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_ALL);
}

static int deferred_push_database_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.path_b,    path, sizeof(info.path_b));
   strlcpy(info.label, label, sizeof(info.label));
   info.path_c[0] = '\0';

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);
}

static int deferred_push_cursor_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   char *query = NULL, *rdb = NULL;
   char rdb_path[PATH_MAX_LENGTH];
   config_file_t *conf    = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();
   settings_t *settings   = config_get_ptr();
   if (!menu)
      return -1;

   conf = config_file_new(path);

   if (!conf)
      return -1;

   if (!config_get_string(conf, "query", &query))
   {
      config_file_free(conf);
      return -1;
   }

   if (!config_get_string(conf, "rdb", &rdb))
   {
      config_file_free(conf);
      return -1;
   }

   fill_pathname_join(rdb_path, settings->content_database,
         rdb, sizeof(rdb_path));

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   rdb_path, sizeof(info.path));
   strlcpy(info.path_b,    path, sizeof(info.path_b));
   strlcpy(info.path_c,    query, sizeof(info.path_c));
   strlcpy(info.label, label, sizeof(info.label));

   menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);

   config_file_free(conf);
   return 0;
}

static int deferred_push_cursor_manager_list_deferred_query_subsearch(
      void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int ret;
   menu_displaylist_info_t info = {0};
   char query[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   bool add_quotes = true;
   file_list_t *list = NULL;
   file_list_t *menu_list = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return -1;

   str_list  = string_split(path, "|"); 

   list                   = (file_list_t*)data;
   menu_list              = (file_list_t*)userdata;

   if (!list || !menu_list)
   {
      string_list_free(str_list);
      return -1;
   }

   strlcpy(query, "{'", sizeof(query));

   if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
      strlcat(query, "publisher", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
      strlcat(query, "developer", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
      strlcat(query, "origin", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
      strlcat(query, "franchise", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
      strlcat(query, "esrb_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
      strlcat(query, "bbfc_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
      strlcat(query, "elspa_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
      strlcat(query, "pegi_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw"))
      strlcat(query, "enhancement_hw", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
      strlcat(query, "cero_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
   {
      strlcat(query, "edge_rating", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
   {
      strlcat(query, "edge_issue", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_famitsu_magazine_rating"))
   {
      strlcat(query, "famitsu_rating", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
   {
      strlcat(query, "releasemonth", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
   {
      strlcat(query, "releaseyear", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
   {
      strlcat(query, "users", sizeof(query));
      add_quotes = false;
   }

   strlcat(query, "':", sizeof(query));
   if (add_quotes)
      strlcat(query, "\"", sizeof(query));
   strlcat(query, str_list->elems[0].data, sizeof(query));
   if (add_quotes)
      strlcat(query, "\"", sizeof(query));
   strlcat(query, "}", sizeof(query));

#if 0
   RARCH_LOG("query: %s\n", query);
#endif

   if (query[0] == '\0')
   {
      string_list_free(str_list);
      return -1;
   }

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   str_list->elems[1].data, sizeof(info.path));
   strlcpy(info.path_b,    str_list->elems[0].data, sizeof(info.path_b));
   strlcpy(info.path_c,    query, sizeof(info.path_c));
   strlcpy(info.label, label, sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);

   string_list_free(str_list);

   return ret;
}

static int deferred_push_performance_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTER_SELECTION);
}

static int deferred_push_video_shader_preset_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PARAMETERS_PRESET);
}

static int deferred_push_video_shader_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PARAMETERS);
}

static int deferred_push_settings(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS_ALL);

}

static int deferred_push_settings_subgroup(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list  = (file_list_t*)data;
   info.type  = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS_SUBGROUP);
}

static int deferred_push_category(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   info.list  = (file_list_t*)data;
   info.type  = type;
   info.flags = SL_FLAG_ALL_SETTINGS;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS);
}

static int deferred_push_video_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_VIDEO);
}

static int deferred_push_shader_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_SHADERS);
}

static int deferred_push_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS);
}

static int deferred_push_management_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_MANAGEMENT);
}

static int deferred_push_core_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTERS_CORE);
}

static int deferred_push_frontend_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTERS_FRONTEND);
}

static int deferred_push_core_cheat_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_CHEATS);
}


static int deferred_push_core_input_remapping_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_REMAPPINGS);
}

static int deferred_push_core_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORE_OPTIONS);
}

static int deferred_push_disk_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_DISK);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

int cb_core_updater_list(void *data_, size_t len)
{
   char *data = (char*)data_;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return -1;

   if (!data)
      return -1;

   if (core_buf)
      free(core_buf);

   core_buf = (char*)malloc(len * sizeof(char));

   if (!core_buf)
      return -1;

   memcpy(core_buf, data, len * sizeof(char));
   core_len = len;

   menu->nonblocking_refresh = false;

   return 0;
}

static int deferred_push_core_updater_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_UPDATER);
}
#endif

static int deferred_push_history_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_HISTORY);
}

static int deferred_push_content_actions(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS);
}

int deferred_push_content_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   return menu_displaylist_push((file_list_t*)data, menu_list->selection_buf);
}

static int deferred_push_database_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings   = config_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_RDB;
   strlcpy(info.exts, "rdb", sizeof(info.exts));
   strlcpy(info.path, settings->content_database, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASES);
}

static int deferred_push_cursor_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings   = config_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CURSOR;
   strlcpy(info.exts, "dbc", sizeof(info.exts));
   strlcpy(info.path, settings->cursor_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_CURSORS);
}

static int deferred_push_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.exts, EXT_EXECUTABLES, sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES);
}

static int deferred_push_configurations(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CONFIG;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CONFIG_FILES);
}

static int deferred_push_video_shader_preset(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_SHADER_PRESET;
   strlcpy(info.exts, "cgp|glslp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PRESET);
}

static int deferred_push_video_shader_pass(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_SHADER;
   strlcpy(info.exts, "cg|glsl", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PASS);
}

static int deferred_push_video_filter(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_VIDEOFILTER;
   strlcpy(info.exts, "filt", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_VIDEO_FILTERS);
}

static int deferred_push_images(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_IMAGE;
   strlcpy(info.exts, "png", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_IMAGES);
}

static int deferred_push_audio_dsp_plugin(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_AUDIOFILTER;
   strlcpy(info.exts, "dsp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_AUDIO_FILTERS);
}

static int deferred_push_cheat_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CHEAT;
   strlcpy(info.exts, "cht", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CHEAT_FILES);
}

static int deferred_push_remap_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_REMAP;
   strlcpy(info.exts, "rmp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_REMAP_FILES);
}

static int deferred_push_record_configfile(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_RECORD_CONFIG;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_RECORD_CONFIG_FILES);
}

static int deferred_push_input_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_OVERLAY;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_input_osk_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_OVERLAY;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_video_font_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_FONT;
   strlcpy(info.exts, "ttf", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_FONTS);
}

static int deferred_push_content_history_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CONTENT_HISTORY);
}

static int deferred_push_detect_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   global_t *global = global_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   if (global->core_info)
      strlcpy(info.exts, core_info_list_get_all_extensions(
         global->core_info), sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));
   
   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_DETECTED);
}

static int deferred_push_default(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   global_t *global         = global_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));
   info.setting      = menu_setting_find(label);

   if (info.setting && info.setting->browser_selection_type == ST_DIR) {}
   else if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         snprintf(info.exts, sizeof(info.exts), "%s",
               global->menu.info.valid_extensions);
   }
   else
      strlcpy(info.exts, global->system.valid_extensions, sizeof(info.exts));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DEFAULT);
}

void menu_entries_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   settings_t *settings   = config_get_ptr();

   if (!cbs)
      return;

   cbs->action_deferred_push = deferred_push_default;

   if ((strlen(elem1) != 0) && !!strcmp(elem0, elem1))
   {
      if (menu_entries_common_is_settings_entry(elem0))
      {
         if (!settings->menu.collapse_subgroups_enable)
         {
            cbs->action_deferred_push = deferred_push_settings_subgroup;
            return;
         }
      }
   }

   if (strstr(label, "deferred_rdb_entry_detail"))
      cbs->action_deferred_push = deferred_push_rdb_entry_detail;
#ifdef HAVE_NETWORKING
   else if (!strcmp(label, "deferred_core_updater_list"))
      cbs->action_deferred_push = deferred_push_core_updater_list;
#endif
   else if (!strcmp(label, "history_list"))
      cbs->action_deferred_push = deferred_push_history_list;
   else if (!strcmp(label, "database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list;
   else if (!strcmp(label, "cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list;
   else if (!strcmp(label, "cheat_file_load"))
      cbs->action_deferred_push = deferred_push_cheat_file_load;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_deferred_push = deferred_push_remap_file_load;
   else if (!strcmp(label, "record_config"))
      cbs->action_deferred_push = deferred_push_record_configfile;
   else if (!strcmp(label, "content_actions"))
      cbs->action_deferred_push = deferred_push_content_actions;
   else if (!strcmp(label, "shader_options"))
      cbs->action_deferred_push = deferred_push_shader_options;
   else if (!strcmp(label, "video_options"))
      cbs->action_deferred_push = deferred_push_video_options;
   else if (!strcmp(label, "options"))
      cbs->action_deferred_push = deferred_push_options;
   else if (!strcmp(label, "management"))
      cbs->action_deferred_push = deferred_push_management_options;
   else if (type == MENU_SETTING_GROUP)
      cbs->action_deferred_push = deferred_push_category;
   else if (!strcmp(label, "deferred_core_list"))
      cbs->action_deferred_push = deferred_push_core_list_deferred;
   else if (!strcmp(label, "deferred_video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "deferred_database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list_deferred;
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred;
   else if (
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_famitsu_magazine_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear")
         )
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred_query_subsearch;
   else if (!strcmp(label, "core_information"))
      cbs->action_deferred_push = deferred_push_core_information;
   else if (!strcmp(label, "system_information"))
      cbs->action_deferred_push = deferred_push_system_information;
   else if (!strcmp(label, "performance_counters"))
      cbs->action_deferred_push = deferred_push_performance_counters;
   else if (!strcmp(label, "core_counters"))
      cbs->action_deferred_push = deferred_push_core_counters;
   else if (!strcmp(label, "video_shader_preset_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_preset_parameters;
   else if (!strcmp(label, "video_shader_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_parameters;
   else if (!strcmp(label, "settings"))
      cbs->action_deferred_push = deferred_push_settings;
   else if (!strcmp(label, "frontend_counters"))
      cbs->action_deferred_push = deferred_push_frontend_counters;
   else if (!strcmp(label, "core_options"))
      cbs->action_deferred_push = deferred_push_core_options;
   else if (!strcmp(label, "core_cheat_options"))
      cbs->action_deferred_push = deferred_push_core_cheat_options;
   else if (!strcmp(label, "core_input_remapping_options"))
      cbs->action_deferred_push = deferred_push_core_input_remapping_options;
   else if (!strcmp(label, "disk_options"))
      cbs->action_deferred_push = deferred_push_disk_options;
   else if (!strcmp(label, "core_list"))
      cbs->action_deferred_push = deferred_push_core_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_deferred_push = deferred_push_configurations;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_deferred_push = deferred_push_video_shader_preset;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_deferred_push = deferred_push_video_shader_pass;
   else if (!strcmp(label, "video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "menu_wallpaper"))
      cbs->action_deferred_push = deferred_push_images;
   else if (!strcmp(label, "audio_dsp_plugin"))
      cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
   else if (!strcmp(label, "input_overlay"))
      cbs->action_deferred_push = deferred_push_input_overlay;
   else if (!strcmp(label, "input_osk_overlay"))
      cbs->action_deferred_push = deferred_push_input_osk_overlay;
   else if (!strcmp(label, "video_font_path"))
      cbs->action_deferred_push = deferred_push_video_font_path;
   else if (!strcmp(label, "game_history_path"))
      cbs->action_deferred_push = deferred_push_content_history_path;
   else if (!strcmp(label, "detect_core_list"))
      cbs->action_deferred_push = deferred_push_detect_core_list;
}

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

#include <stddef.h>

#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

#include "menu.h"
#include "menu_hash.h"
#include "menu_display.h"
#include "menu_displaylist.h"
#include "menu_navigation.h"
#include "menu_setting.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../general.h"
#include "../retroarch.h"
#include "../gfx/video_shader_driver.h"
#include "../config.features.h"
#include "../git_version.h"
#include "../performance.h"

#ifdef ANDROID
#include "../frontend/drivers/platform_android.h"
#endif

#ifdef HAVE_NETWORKING
extern char *core_buf;
extern size_t core_len;

static void print_buf_lines(file_list_t *list, char *buf, int buf_size,
      unsigned type)
{
   int i, j = 0;
   char c;
   char *line_start = buf;

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;

      /* The end of the buffer, print the last bit */
      if (*(buf + i) == '\0')
         break;

      if (*(buf + i) != '\n')
         continue;

      /* Found a line ending, print the line and compute new line_start */

      /* Save the next char  */
      c = *(buf + i + 1);
      /* replace with \0 */
      *(buf + i + 1) = '\0';

      /* We need to strip the newline. */
      ln = strlen(line_start) - 1;
      if (line_start[ln] == '\n')
         line_start[ln] = '\0';

      menu_list_push(list, line_start, "",
            type, 0, 0);
      if (type == MENU_FILE_DOWNLOAD_CORE)
      {
         char core_path[PATH_MAX_LENGTH]    = {0};
         char display_name[PATH_MAX_LENGTH] = {0};
         settings_t *settings      = config_get_ptr();

         if (settings)
         {
            char *last = NULL;
            fill_pathname_join(core_path, settings->libretro_info_path,
                  line_start, sizeof(core_path));
            
            (void)last;

            path_remove_extension(core_path);
            path_remove_extension(core_path);
#ifdef ANDROID
            last = (char*)strrchr(core_path, '_');
            if (*last)
               *last = '\0';
#endif
            strlcat(core_path, ".info", sizeof(core_path));

            if (core_info_get_display_name(
                     core_path, display_name, sizeof(display_name)))
               menu_list_set_alt_at_offset(list, j, display_name);
         }
      }
      j++;

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start = buf + i + 1;
   }
   /* If the buffer was completely full, and didn't end with a newline, just
    * ignore the partial last line.
    */
}
#endif

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS),
            "", 0, 0, 0);
      return;
   }

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(info->list,
               counters[i]->ident, "", id + i, 0, 0);
}

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info)
{
   unsigned i;
   char tmp[PATH_MAX_LENGTH] = {0};
   settings_t *settings      = config_get_ptr();
   global_t *global          = global_get_ptr();
   core_info_t *core_info    = global ? (core_info_t*)global->core_info_current : NULL;

   if (!core_info || !core_info->data)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            "", 0, 0, 0);
      return 0;
   }

   strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CORE_NAME), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->core_name)
      strlcat(tmp, core_info->core_name, sizeof(tmp));

   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->display_name)
      strlcat(tmp, core_info->display_name, sizeof(tmp));
   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   if (core_info->systemname)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, core_info->systemname, sizeof(tmp));
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->system_manufacturer)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, core_info->system_manufacturer, sizeof(tmp));
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->categories_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CATEGORIES), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->categories_list, ", ");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->authors_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_AUTHORS), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->authors_list, ", ");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->permissions_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->permissions_list, ", ");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->licenses_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_LICENSES), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->licenses_list, ", ");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->supported_extensions_list)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->supported_extensions_list, ", ");
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->firmware_count > 0)
   {
      core_info_list_update_missing_firmware(
            global->core_info, core_info->path,
            settings->system_directory);

      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_FIRMWARE), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      /* FIXME: This looks hacky and probably needs to be improved for good translation support. */
      for (i = 0; i < core_info->firmware_count; i++)
      {
         if (core_info->firmware[i].desc)
         {
            snprintf(tmp, sizeof(tmp), "	%s: %s",
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_NAME),
                  core_info->firmware[i].desc ?
                  core_info->firmware[i].desc : "");
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

            snprintf(tmp, sizeof(tmp), "	%s: %s, %s",
                  menu_hash_to_str(MENU_VALUE_STATUS),
                  core_info->firmware[i].missing ?
                  menu_hash_to_str(MENU_VALUE_MISSING) :
                  menu_hash_to_str(MENU_VALUE_PRESENT),
                  core_info->firmware[i].optional ?
                  menu_hash_to_str(MENU_VALUE_OPTIONAL) :
                  menu_hash_to_str(MENU_VALUE_REQUIRED)
                  );
            menu_list_push(info->list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
         }
      }
   }

   if (core_info->notes)
   {
      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      for (i = 0; i < core_info->note_list->size; i++)
      {
         strlcpy(tmp,
               core_info->note_list->elems[i].data, sizeof(tmp));
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   char feat_str[PATH_MAX_LENGTH]        = {0};
   char tmp[PATH_MAX_LENGTH]             = {0};
   char tmp2[PATH_MAX_LENGTH]            = {0};
   const char *tmp_string                = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();

   snprintf(tmp, sizeof(tmp), "%s: %s", menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE), __DATE__);
   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   (void)tmp_string;

#ifdef HAVE_GIT_VERSION
   strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, rarch_git_version, sizeof(tmp));
   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
#endif

   rarch_info_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   menu_list_push(info->list, tmp, "", MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#ifdef ANDROID
   bool perms = test_permissions(sdcard_dir);

   snprintf(tmp, sizeof(tmp), "%s: %s", "Internal SD card status", perms ? "read-write" : "read-only");
   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#endif
   {
      char cpu_str[PATH_MAX_LENGTH] = {0};

      strlcpy(cpu_str, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES), sizeof(cpu_str));
      strlcat(cpu_str, ": ", sizeof(cpu_str));

      rarch_info_get_capabilities(RARCH_CAPABILITIES_CPU, cpu_str, sizeof(cpu_str));
      menu_list_push(info->list, cpu_str, "", MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (frontend)
   {
      int major = 0, minor = 0;

      strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, frontend->ident, sizeof(tmp));
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));
         strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME), sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, frontend->get_name ? tmp2 : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), sizeof(tmp));
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s : %s %d.%d",
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS),
               frontend->get_os ? tmp2 : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), major, minor);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      snprintf(tmp, sizeof(tmp), "%s : %d",
            menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
            frontend->get_rating ? frontend->get_rating() : -1);
      menu_list_push(info->list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

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
               strlcat(tmp2, " ", sizeof(tmp));
               strlcat(tmp2, menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_NO_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE), sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGING:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING), sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGED:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED), sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING), sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
         }

         strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE), sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, tmp2, sizeof(tmp));
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
   tmp_string = gfx_ctx_get_ident();

   strlcpy(tmp, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER), sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, tmp_string ? tmp_string : menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), sizeof(tmp));
   menu_list_push(info->list, tmp, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   {
      float val = 0.0f;
      if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_WIDTH, &val))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH), val);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_HEIGHT, &val))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH), val);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &val))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI), val);
         menu_list_push(info->list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }
#endif

   strlcpy(feat_str, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT), sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _libretrodb_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO), sizeof(feat_str));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT), sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _overlay_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO), sizeof(feat_str));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT), sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _command_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO), sizeof(feat_str));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s : %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT),
         _network_command_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT),
          _cocoa_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT),
         _rpng_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT),
         _sdl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT),
         _sdl2_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);


   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT),
         _opengl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT),
         _opengles_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT),
         _thread_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT),
         _kms_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT),
         _udev_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT),
         _vg_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT),
         _egl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT),
         _x11_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT),
         _wayland_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT),
         _xvideo_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT),
         _alsa_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT),
         _oss_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT),
         _al_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT),
         _sl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT),
         _rsound_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT),
         _roar_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT),
         _jack_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT),
         _pulse_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT),
         _dsound_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT),
         _xaudio_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT),
         _zlib_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT),
         _7zip_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT),
         _dylib_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT),
         _cg_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT),
         _glsl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT),
         _hlsl_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT),
         _libxml2_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT),
         _sdl_image_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT),
         _fbo_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT),
         _ffmpeg_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT),
         _coretext_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT),
         _freetype_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT),
         _netplay_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT),
         _python_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT),
         _v4l2_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT),
         _libusb_supp ? menu_hash_to_str(MENU_LABEL_VALUE_YES) : menu_hash_to_str(MENU_LABEL_VALUE_NO));
   menu_list_push(info->list, feat_str, "",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   return 0;
}

static int menu_displaylist_parse_playlist(menu_displaylist_info_t *info,
      content_playlist_t *playlist, const char *path_playlist, bool is_history)
{
   unsigned i;
   size_t list_size = 0;
   settings_t *settings          = config_get_ptr();

   if (!playlist)
      return -1;

   list_size = content_playlist_size(playlist);

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            menu_hash_to_str(MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            0, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      uint32_t core_name_hash;
      char fill_buf[PATH_MAX_LENGTH]  = {0};
      char path_copy[PATH_MAX_LENGTH] = {0};
      bool core_detected              = false;
      const char *core_name           = NULL;
      const char *db_name             = NULL;
      const char *path                = NULL;
      const char *label               = NULL;
      const char *crc32               = NULL;

      strlcpy(path_copy, info->path, sizeof(path_copy));

      path = path_copy;

      content_playlist_get_index(playlist, i,
            &path, &label, NULL, &core_name, &crc32, &db_name);
      strlcpy(fill_buf, core_name, sizeof(fill_buf));

      core_name_hash = core_name ? menu_hash_calculate(core_name) : 0;

      if (path)
      {
         char path_short[PATH_MAX_LENGTH] = {0};

         fill_short_pathname_representation(path_short, path,
               sizeof(path_short));
         strlcpy(fill_buf, 
               (label && label[0] != '\0') ? label : path_short,
               sizeof(fill_buf));

         if (core_name && core_name[0] != '\0')
         {
            if (core_name_hash != MENU_VALUE_DETECT)
            {
               char tmp[PATH_MAX_LENGTH] = {0};
               snprintf(tmp, sizeof(tmp), " (%s)", core_name);
               strlcat(fill_buf, tmp, sizeof(fill_buf));
               core_detected = true;
            }
         }
      }

      if (!is_history && core_detected && db_name && db_name[0] != '\0')
      {
         char db_path[PATH_MAX_LENGTH] = {0};

         fill_pathname_join(db_path, settings->content_database,
               db_name, sizeof(db_path));
         path_remove_extension(db_path);
         strlcat(db_path, ".rdb", sizeof(db_path));

         menu_list_push(info->list, label,
               db_path, MENU_FILE_RDB_ENTRY, 0, i);
      }
      else
         menu_list_push(info->list, fill_buf, path_playlist,
               MENU_FILE_PLAYLIST_ENTRY, 0, i);
   }

   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   struct video_shader *shader = NULL;
   menu_handle_t         *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   shader = menu->shader;

   if (!shader)
      return -1;

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SHADER_APPLY_CHANGES),
         menu_hash_to_str(MENU_LABEL_SHADER_APPLY_CHANGES),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET),
         MENU_FILE_PATH, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PARAMETERS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_NUM_PASSES),
         0, 0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf_tmp[64] = {0};
      char buf[64] = {0};

      snprintf(buf_tmp, sizeof(buf_tmp), "%s #%u", menu_hash_to_str(MENU_VALUE_SHADER), i);

      menu_list_push(info->list, buf_tmp,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PASS),
            MENU_SETTINGS_SHADER_PASS_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s Filter", buf_tmp);
      menu_list_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_FILTER_PASS),
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s Scale", buf_tmp);
      menu_list_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_SCALE_PASS),
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0, 0);
   }

   return 0;
}

#ifdef HAVE_LIBRETRODB
static int create_string_list_rdb_entry_string(const char *desc, const char *label,
      const char *actual_string, const char *path, file_list_t *list)
{
   union string_list_elem_attr attr = {0};
   char tmp[PATH_MAX_LENGTH]    = {0};
   char *output_label           = NULL;
   int str_len                  = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

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

   strlcpy(tmp, desc, sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, actual_string, sizeof(tmp));
   menu_list_push(list, tmp, output_label, 0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int create_string_list_rdb_entry_int(const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   union string_list_elem_attr attr = {0};
   char tmp[PATH_MAX_LENGTH]    = {0};
   char str[PATH_MAX_LENGTH]    = {0};
   char *output_label           = NULL;
   int str_len                  = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

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

   snprintf(tmp, sizeof(tmp), "%s : %d", desc, actual_int);
   menu_list_push(list, tmp, output_label,
         0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}
#endif

static int menu_displaylist_parse_database_entry(menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   unsigned i, j, k;
   content_playlist_t *playlist        = NULL;
   database_info_list_t *db_info       = NULL;
   char path_playlist[PATH_MAX_LENGTH] = {0};
   char path_base[PATH_MAX_LENGTH]     = {0};
   char query[PATH_MAX_LENGTH]         = {0};
   menu_handle_t *menu                 = menu_driver_get_ptr();
   settings_t *settings                = config_get_ptr();
   if (!menu)
      goto error;

   database_info_build_query(query, sizeof(query), "displaylist_parse_database_entry", info->path_b);

   if (!(db_info = database_info_list_new(info->path, query)))
      goto error;

   fill_short_pathname_representation(path_base, info->path,
         sizeof(path_base));
   path_remove_extension(path_base);
   strlcat(path_base, ".lpl", sizeof(path_base));

   fill_pathname_join(path_playlist, settings->playlist_directory, path_base,
         sizeof(path_playlist));

   playlist = content_playlist_init(path_playlist, 1000);

   if (playlist)
      strlcpy(menu->db_playlist_file, path_playlist,
            sizeof(menu->db_playlist_file));

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH]      = {0};
      char crc_str[20]               = {0};
      database_info_t *db_info_entry = &db_info->list[i];
      settings_t *settings           = config_get_ptr();
      bool show_advanced_settings    = settings ? settings->menu.show_advanced_settings : false;

      if (!db_info_entry)
         continue;

      snprintf(crc_str, sizeof(crc_str), "%08X", db_info_entry->crc32);

      if (playlist)
      {
         for (j = 0; j < playlist->size; j++)
         {
            uint32_t core_name_hash, core_path_hash;
            char elem0[PATH_MAX_LENGTH]      = {0};
            char elem1[PATH_MAX_LENGTH]      = {0};
            bool match_found                 = false;
            struct string_list *tmp_str_list = string_split(
                  playlist->entries[j].crc32, "|");
            uint32_t hash_value              = 0;

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
               strlcpy(elem0, tmp_str_list->elems[0].data, sizeof(elem0));
            if (tmp_str_list->size > 1)
               strlcpy(elem1, tmp_str_list->elems[1].data, sizeof(elem1));

            hash_value = menu_hash_calculate(elem1);

            switch (hash_value)
            {
               case MENU_VALUE_CRC:
                  if (!strcmp(crc_str, elem0))
                     match_found = true;
                  break;
               case MENU_VALUE_SHA1:
                  if (!strcmp(db_info_entry->sha1, elem0))
                     match_found = true;
                  break;
               case MENU_VALUE_MD5:
                  if (!strcmp(db_info_entry->md5, elem0))
                     match_found = true;
                  break;
            }

            string_list_free(tmp_str_list);

            if (!match_found)
               continue;

            rdb_entry_start_game_selection_ptr = j;

            core_name_hash = menu_hash_calculate(playlist->entries[j].core_name);
            core_path_hash = menu_hash_calculate(playlist->entries[j].core_path);

            if (
                  (core_name_hash != MENU_VALUE_DETECT) &&
                  (core_path_hash != MENU_VALUE_DETECT)
               )
               menu_list_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT),
                     menu_hash_to_str(MENU_LABEL_RDB_ENTRY_START_CONTENT),
                     MENU_FILE_PLAYLIST_ENTRY, 0, 0);
         }
      }

      if (db_info_entry->name)
      {
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_NAME),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, db_info_entry->name, sizeof(tmp));
         menu_list_push(info->list, tmp,
               menu_hash_to_str(MENU_LABEL_RDB_ENTRY_NAME),
               0, 0, 0);
      }
      if (db_info_entry->description)
      {
         strlcpy(tmp,
               menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION),
               sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, db_info_entry->description, sizeof(tmp));
         menu_list_push(info->list, tmp,
               menu_hash_to_str(MENU_LABEL_RDB_ENTRY_DESCRIPTION),
               0, 0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_PUBLISHER),
                  db_info_entry->publisher, info->path, info->list) == -1)
            goto error;
      }


      if (db_info_entry->developer)
      {
         for (k = 0; k < db_info_entry->developer->size; k++)
         {
            if (db_info_entry->developer->elems[k].data)
            {
               if (create_string_list_rdb_entry_string(
                        menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER),
                        menu_hash_to_str(MENU_LABEL_RDB_ENTRY_DEVELOPER),
                        db_info_entry->developer->elems[k].data,
                        info->path, info->list) == -1)
                  goto error;
            }
         }
      }

      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ORIGIN),
                  db_info_entry->origin, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_FRANCHISE),
                  db_info_entry->franchise, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_INPUT_MAX_USERS),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_MAX_USERS),
                  db_info_entry->max_users,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Famitsu Magazine Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING),
                  db_info_entry->famitsu_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_review)
      {
         if (create_string_list_rdb_entry_string("Edge Magazine Review",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW),
                  db_info_entry->edge_magazine_review, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING),
                  db_info_entry->edge_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Issue",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE),
                  db_info_entry->edge_magazine_issue,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RELEASE_MONTH),
                  db_info_entry->releasemonth,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RELEASE_YEAR),
                  db_info_entry->releaseyear,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string("BBFC Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_BBFC_RATING),
                  db_info_entry->bbfc_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string("ESRB Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ESRB_RATING),
                  db_info_entry->esrb_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string("ELSPA Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ELSPA_RATING),
                  db_info_entry->elspa_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string("PEGI Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_PEGI_RATING),
                  db_info_entry->pegi_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string("Enhancement Hardware",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW),
                  db_info_entry->enhancement_hw, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string("CERO Rating",
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_CERO_RATING),
                  db_info_entry->cero_rating, info->path, info->list) == -1)
            goto error;
      }
      snprintf(tmp, sizeof(tmp),
            "%s : %s",
            "Analog supported",
            (db_info_entry->analog_supported == 1)  ? menu_hash_to_str(MENU_VALUE_TRUE) :
            (db_info_entry->analog_supported == -1) ? menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE)  :
            menu_hash_to_str(MENU_VALUE_FALSE));
      menu_list_push(info->list, tmp,
            menu_hash_to_str(MENU_LABEL_RDB_ENTRY_ANALOG),
            0, 0, 0);
      snprintf(tmp, sizeof(tmp),
            "%s : %s",
            "Rumble supported",
            (db_info_entry->rumble_supported == 1)  ? menu_hash_to_str(MENU_VALUE_TRUE) :
            (db_info_entry->rumble_supported == -1) ? menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE)  :
            menu_hash_to_str(MENU_VALUE_FALSE));
      menu_list_push(info->list, tmp,
            menu_hash_to_str(MENU_LABEL_RDB_ENTRY_RUMBLE),
            0, 0, 0);

      if (!show_advanced_settings)
         continue;

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_CRC32),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_CRC32),
                  crc_str,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_SHA1),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_SHA1),
                  db_info_entry->sha1,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string(
                  menu_hash_to_str(MENU_LABEL_VALUE_RDB_ENTRY_MD5),
                  menu_hash_to_str(MENU_LABEL_RDB_ENTRY_MD5),
                  db_info_entry->md5,
                  info->path, info->list) == -1)
            goto error;
      }
   }

   if (db_info->count < 1)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            menu_hash_to_str(MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            0, 0, 0);

   content_playlist_free(playlist);
   database_info_list_free(db_info);

   return 0;

error:
   content_playlist_free(playlist);

   return -1;
#else
   return 0;
#endif
}

static int menu_database_parse_query(file_list_t *list, const char *path,
      const char *query)
{
#ifdef HAVE_LIBRETRODB
   unsigned i;
   database_info_list_t *db_list = database_info_list_new(path, query);

   if (!db_list)
      return -1;

   for (i = 0; i < db_list->count; i++)
   {
      if (db_list->list[i].name && db_list->list[i].name[0] != '\0')
         menu_list_push(list, db_list->list[i].name,
               path, MENU_FILE_RDB_ENTRY, 0, 0);
   }

   database_info_list_free(db_list);
#endif

   return 0;
}

#ifdef HAVE_SHADER_MANAGER
static int deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   size_t list_size = shader->num_parameters;

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_SHADER_PARAMETERS),
            "", 0, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
      menu_list_push(info->list, shader->parameters[i].desc,
            info->label, base_parameter + i, 0, 0);

   return 0;
}
#endif

static void menu_displaylist_realloc_settings(menu_entries_t *entries, unsigned flags)
{
   if (!entries)
      return;

   if (entries->list_settings)
      menu_setting_free(entries->list_settings);

   entries->list_settings      = menu_setting_new(flags);
}

static int menu_displaylist_parse_settings(menu_handle_t *menu,
      menu_displaylist_info_t *info, unsigned setting_flags)
{
   size_t             count = 0;
   rarch_setting_t *setting = NULL;
   settings_t *settings     = config_get_ptr();

   menu_displaylist_realloc_settings(&menu->entries, setting_flags);

   setting                  = menu_setting_find(info->label);

   if (!setting)
      return -1;


   for (; setting->type != ST_END_GROUP; setting++)
   {
      switch (setting->type)
      {
         case ST_GROUP:
         case ST_SUB_GROUP:
         case ST_END_SUB_GROUP:
            continue;
         default:
            break;
      }

      if (setting->flags & SD_FLAG_ADVANCED &&
            !settings->menu.show_advanced_settings)
         continue;

      menu_list_push(info->list, setting->short_description,
            setting->name, menu_setting_set_flags(setting), 0, 0);
      count++;
   }

   if (count == 0)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_SETTINGS_FOUND),
            menu_hash_to_str(MENU_LABEL_NO_SETTINGS_FOUND),
            0, 0, 0);

   return 0;
}

static int menu_displaylist_parse_settings_in_subgroup(menu_displaylist_info_t *info)
{
   char elem0[PATH_MAX_LENGTH]  = {0};
   char elem1[PATH_MAX_LENGTH]  = {0};
   struct string_list *str_list = NULL;
   menu_handle_t          *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   if (info->label[0] != '\0')
   {
      str_list = string_split(info->label, "|");

      if (str_list && str_list->size > 0)
         strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
      if (str_list && str_list->size > 1)
         strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

      if (str_list)
      {
         string_list_free(str_list);
         str_list = NULL;
      }
   }

   menu_displaylist_realloc_settings(&menu->entries, SL_FLAG_ALL_SETTINGS);

   info->setting = menu_setting_find(elem0);

   if (!info->setting)
      return -1;

   while (1)
   {
      if (info->setting->type == ST_SUB_GROUP)
      {
         if ((strlen(info->setting->name) != 0) && !strcmp(info->setting->name, elem1))
            break;
      }
      info->setting++;
   }


   info->setting++;

   for (; info->setting->type != ST_END_SUB_GROUP; info->setting++)
      menu_list_push(info->list, info->setting->short_description,
            info->setting->name, menu_setting_set_flags(info->setting), 0, 0);

   return 0;
}

#if 0
static void menu_displaylist_push_horizontal_menu_list_content(
      file_list_t *list, core_info_t *info, const char* path)
{
   unsigned j;
   struct string_list *str_list =
      dir_list_new(path, info->supported_extensions, true);

   if (!str_list)
      return;

   dir_list_sort(str_list, true);

   for (j = 0; j < str_list->size; j++)
   {
      const char *name = str_list->elems[j].data;

      if (!name)
         continue;

      if (str_list->elems[j].attr.i == RARCH_DIRECTORY)
         menu_displaylist_push_horizontal_menu_list_content(list, info, name);
      else
         menu_list_push(
               list, name,
               "content_actions",
               MENU_FILE_CONTENTLIST_ENTRY, 0);
   }

   string_list_free(str_list);
}
#endif

static int menu_displaylist_sort_playlist(const content_playlist_entry_t *a,
      const content_playlist_entry_t *b)
{
   if (!a->label || !b->label)
      return 0;

   return strcasecmp(a->label, b->label);
}

static int menu_displaylist_parse_horizontal_list(menu_displaylist_info_t *info)
{
   char path_playlist[PATH_MAX_LENGTH] = {0};
   char lpl_basename[PATH_MAX_LENGTH]  = {0};
   content_playlist_t *playlist        = NULL;
   settings_t      *settings           = config_get_ptr();
   menu_handle_t        *menu          = menu_driver_get_ptr();
   struct item_file *item              = (struct item_file*)
      menu_driver_list_get_entry(MENU_LIST_HORIZONTAL, menu_driver_list_get_selection() - 1);

   if (!item)
      return -1;

   strlcpy(lpl_basename, item->path, sizeof(lpl_basename));
   path_remove_extension(lpl_basename);

   if (menu->playlist)
      content_playlist_free(menu->playlist);

   fill_pathname_join(path_playlist,
         settings->playlist_directory, item->path,
         sizeof(path_playlist));
   menu->playlist  = content_playlist_init(path_playlist,
         999);
   strlcpy(menu->db_playlist_file, path_playlist, sizeof(menu->db_playlist_file));
   strlcpy(path_playlist,
         menu_hash_to_str(MENU_LABEL_COLLECTION),
         sizeof(path_playlist));
   playlist = menu->playlist;

   content_playlist_qsort(playlist, menu_displaylist_sort_playlist);

   menu_displaylist_parse_playlist(info, playlist, path_playlist, false);

   return 0;
}

static int menu_displaylist_parse_load_content_settings(menu_displaylist_info_t *info)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   global_t *global       = global_get_ptr();
   if (!menu)
      return -1;

   if (global->main_is_init && (global->core_type != CORE_TYPE_DUMMY))
   {
      rarch_system_info_t *system = rarch_system_info_get_ptr();

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_RESUME_CONTENT),
            menu_hash_to_str(MENU_LABEL_RESUME_CONTENT),
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_RESTART_CONTENT),
            menu_hash_to_str(MENU_LABEL_RESTART_CONTENT),
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CLOSE_CONTENT),
            menu_hash_to_str(MENU_LABEL_CLOSE_CONTENT),
            MENU_SETTING_ACTION_CLOSE, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_TAKE_SCREENSHOT),
            menu_hash_to_str(MENU_LABEL_TAKE_SCREENSHOT),
            MENU_SETTING_ACTION_SCREENSHOT, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SAVE_STATE),
            menu_hash_to_str(MENU_LABEL_SAVE_STATE),
            MENU_SETTING_ACTION_SAVESTATE, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_LOAD_STATE),
            menu_hash_to_str(MENU_LABEL_LOAD_STATE),
            MENU_SETTING_ACTION_LOADSTATE, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_OPTIONS),
            menu_hash_to_str(MENU_LABEL_CORE_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);

      if (global->has_set_input_descriptors)
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS),
               menu_hash_to_str(MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS),
               MENU_SETTING_ACTION, 0, 0);
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS),
            menu_hash_to_str(MENU_LABEL_CORE_CHEAT_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);
      if ((global->core_type != CORE_TYPE_DUMMY) && system && system->disk_control.get_num_images)
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_OPTIONS),
               menu_hash_to_str(MENU_LABEL_DISK_OPTIONS),
               MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0);
#ifdef HAVE_SHADER_MANAGER
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SHADER_OPTIONS),
            menu_hash_to_str(MENU_LABEL_SHADER_OPTIONS),
            MENU_SETTING_ACTION, 0, 0);
#endif
   }
   else
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
            "", 0, 0, 0);

   return 0;
}

static int menu_displaylist_parse_information_list(menu_displaylist_info_t *info)
{
   global_t *global            = global_get_ptr();

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFORMATION),
         menu_hash_to_str(MENU_LABEL_CORE_INFORMATION),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFORMATION),
         menu_hash_to_str(MENU_LABEL_SYSTEM_INFORMATION),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
   menu_list_push(info->list, menu_hash_to_str(MENU_LABEL_VALUE_DATABASE_MANAGER),
         menu_hash_to_str(MENU_LABEL_DATABASE_MANAGER_LIST),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list, menu_hash_to_str(MENU_LABEL_VALUE_CURSOR_MANAGER),
         menu_hash_to_str(MENU_LABEL_CURSOR_MANAGER_LIST),
         MENU_SETTING_ACTION, 0, 0);
#endif

   if (global->perfcnt_enable)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_FRONTEND_COUNTERS),
            menu_hash_to_str(MENU_LABEL_FRONTEND_COUNTERS),
            MENU_SETTING_ACTION, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_CORE_COUNTERS),
            menu_hash_to_str(MENU_LABEL_CORE_COUNTERS),
            MENU_SETTING_ACTION, 0, 0);
   }

   return 0;
}

static int menu_displaylist_parse_add_content_list(menu_displaylist_info_t *info)
{
   global_t *global            = global_get_ptr();

   (void)global;

#ifdef HAVE_NETWORKING
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         menu_hash_to_str(MENU_LABEL_DOWNLOAD_CORE_CONTENT),
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_LIBRETRODB
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SCAN_DIRECTORY),
         menu_hash_to_str(MENU_LABEL_SCAN_DIRECTORY),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SCAN_FILE),
         menu_hash_to_str(MENU_LABEL_SCAN_FILE),
         MENU_SETTING_ACTION, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_load_content_list(menu_displaylist_info_t *info)
{
   global_t *global            = global_get_ptr();

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT),
         menu_hash_to_str(MENU_LABEL_LOAD_CONTENT),
         MENU_SETTING_ACTION, 0, 0);

   if (global->core_info && core_info_list_num_info_files(global->core_info))
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_DETECT_CORE_LIST),
            menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST),
            MENU_SETTING_ACTION, 0, 0);

      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
            menu_hash_to_str(MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
            MENU_SETTING_ACTION, 0, 0);
   }

#ifdef HAVE_LIBRETRODB
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST),
         menu_hash_to_str(MENU_LABEL_CONTENT_COLLECTION_LIST),
         MENU_SETTING_ACTION, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_options(menu_displaylist_info_t *info)
{
#ifdef HAVE_NETWORKING
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST),
         menu_hash_to_str(MENU_LABEL_CORE_UPDATER_LIST),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES),
         menu_hash_to_str(MENU_LABEL_UPDATE_CORE_INFO_FILES),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_ASSETS),
         menu_hash_to_str(MENU_LABEL_UPDATE_ASSETS),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES),
         menu_hash_to_str(MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES),
         MENU_SETTING_ACTION, 0, 0);

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CHEATS),
         menu_hash_to_str(MENU_LABEL_UPDATE_CHEATS),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_DATABASES),
         menu_hash_to_str(MENU_LABEL_UPDATE_DATABASES),
         MENU_SETTING_ACTION, 0, 0);
#endif

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_OVERLAYS),
         menu_hash_to_str(MENU_LABEL_UPDATE_OVERLAYS),
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_CG
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_CG_SHADERS),
         menu_hash_to_str(MENU_LABEL_UPDATE_CG_SHADERS),
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_GLSL
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS),
         menu_hash_to_str(MENU_LABEL_UPDATE_GLSL_SHADERS),
         MENU_SETTING_ACTION, 0, 0);
#endif
#else
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
         "", 0, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_options_cheats(menu_displaylist_info_t *info)
{
   unsigned i;
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global ? global->cheat : NULL;

   if (!cheat)
   {
      global->cheat = cheat_manager_new(0);

      if (!global->cheat)
         return -1;
      cheat = global->cheat;
   }

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_NUM_PASSES),
         menu_hash_to_str(MENU_LABEL_CHEAT_NUM_PASSES),
         0, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES),
         menu_hash_to_str(MENU_LABEL_CHEAT_APPLY_CHANGES),
         MENU_SETTING_ACTION, 0, 0);

   for (i = 0; i < cheat->size; i++)
   {
      char cheat_label[64] = {0};

      snprintf(cheat_label, sizeof(cheat_label), "%s #%u: ", menu_hash_to_str(MENU_VALUE_CHEAT), i);
      if (cheat->cheats[i].desc)
         strlcat(cheat_label, cheat->cheats[i].desc, sizeof(cheat_label));
      menu_list_push(info->list, cheat_label, "", MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_remappings(menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   settings_t        *settings = config_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_SAVE_CORE),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_SAVE_GAME),
         MENU_SETTING_ACTION, 0, 0);

   for (p = 0; p < settings->input.max_users; p++)
   {
      for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND + 4; retro_id++)
      {
         char desc_label[64]     = {0};
         unsigned user           = p + 1;
         unsigned desc_offset    = retro_id;
         const char *description = NULL;

         if (desc_offset >= RARCH_FIRST_CUSTOM_BIND)
            desc_offset = RARCH_FIRST_CUSTOM_BIND + (desc_offset - RARCH_FIRST_CUSTOM_BIND) * 2;

         description = system ? system->input_desc_btn[p][desc_offset] : NULL;

         if (!description)
            continue;

         snprintf(desc_label, sizeof(desc_label),
               "%s %u %s : ", menu_hash_to_str(MENU_VALUE_USER), user, description);
         menu_list_push(info->list, desc_label, "",
               MENU_SETTINGS_INPUT_DESC_BEGIN +
               (p * (RARCH_FIRST_CUSTOM_BIND + 4)) +  retro_id, 0, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_generic(menu_displaylist_info_t *info, bool *need_sort)
{
   bool path_is_compressed, push_dir, filter_ext;
   size_t i, list_size;
   struct string_list *str_list = NULL;
   int                   device = 0;
   menu_list_t *menu_list       = menu_list_get_ptr();
   global_t *global             = global_get_ptr();
   settings_t *settings         = config_get_ptr();
   uint32_t hash_label          = menu_hash_calculate(info->label);

   (void)device;

   if (!*info->path)
   {
      if (frontend_driver_parse_drive_list(info->list) != 0)
         menu_list_push(info->list, "/", "",
               MENU_FILE_DIRECTORY, 0, 0);
      return 0;
   }

#if defined(GEKKO) && defined(HW_RVL)
   slock_lock(gx_device_mutex);
   device = gx_get_device_from_path(info->path);

   if (device != -1 && !gx_devices[device].mounted &&
         gx_devices[device].interface->isInserted())
      fatMountSimple(gx_devices[device].name, gx_devices[device].interface);

   slock_unlock(gx_device_mutex);
#endif

   path_is_compressed = path_is_compressed_file(info->path);
   push_dir           = (info->setting
         && info->setting->browser_selection_type == ST_DIR);

   filter_ext = settings->menu.navigation.browser.filter.supported_extensions_enable;

   if (hash_label == MENU_LABEL_SCAN_FILE)
      filter_ext = false;

   if (path_is_compressed)
      str_list = compressed_file_list_new(info->path, info->exts);
   else
      str_list = dir_list_new(info->path,
            filter_ext ? info->exts : NULL,
            true);

   if (hash_label == MENU_LABEL_SCAN_DIRECTORY)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY),
            menu_hash_to_str(MENU_LABEL_SCAN_THIS_DIRECTORY),
            MENU_FILE_SCAN_DIRECTORY, 0 ,0);

   if (push_dir)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_USE_THIS_DIRECTORY),
            menu_hash_to_str(MENU_LABEL_USE_THIS_DIRECTORY),
            MENU_FILE_USE_DIRECTORY, 0 ,0);

   if (!str_list)
   {
      const char *str = path_is_compressed
         ? menu_hash_to_str(MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE)
         : menu_hash_to_str(MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      menu_list_push(info->list, str, "", 0, 0, 0);
      return 0;
   }

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size <= 0)
   {
      if (!(info->flags & SL_FLAG_ALLOW_EMPTY_LIST))
      {
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
               "", 0, 0, 0);
      }

      string_list_free(str_list);

      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      const char *path            = NULL;
      char label[PATH_MAX_LENGTH] = {0};
      menu_file_type_t file_type  = MENU_FILE_NONE;

      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = MENU_FILE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = MENU_FILE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = MENU_FILE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            if (hash_label == MENU_LABEL_DETECT_CORE_LIST ||
                  hash_label == MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST)
            {
               if (path_is_compressed_file(str_list->elems[i].data))
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  file_type = MENU_FILE_CARCHIVE;
                  break;
               }
            }
            file_type = (menu_file_type_t)info->type_default;
            break;
      }

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;
      if (hash_label == MENU_LABEL_SCAN_DIRECTORY && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (*info->path && !path_is_compressed)
         path = path_basename(path);

      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      switch (hash_label)
      {
         case MENU_LABEL_CONTENT_COLLECTION_LIST:
            if (is_dir)
                continue;

            file_type = MENU_FILE_PLAYLIST_COLLECTION;
            break;
         case MENU_LABEL_CORE_LIST:
#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
            if (is_dir || strcasecmp(path, SALAMANDER_FILE) == 0)
               continue;
#endif
#endif
            /* Compressed cores are unsupported */
            if (file_type == MENU_FILE_CARCHIVE)
               continue;

            file_type = is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE;
            break;
      }

      if (settings->multimedia.builtin_mediaplayer_enable ||
            settings->multimedia.builtin_imageviewer_enable)
      {
         switch (rarch_path_is_media_type(path))
         {
            case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
               if (settings->multimedia.builtin_mediaplayer_enable)
                  file_type = MENU_FILE_MOVIE;
#endif
               break;
            case RARCH_CONTENT_MUSIC:
#ifdef HAVE_FFMPEG
               if (settings->multimedia.builtin_mediaplayer_enable)
                  file_type = MENU_FILE_MUSIC;
#endif
               break;
            case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
               if (settings->multimedia.builtin_imageviewer_enable
                     && hash_label != MENU_LABEL_MENU_WALLPAPER)
                  file_type = MENU_FILE_IMAGEVIEWER;
#endif
            default:
               break;
         }
      }

      menu_list_push(info->list, path, label,
            file_type, 0, 0);
   }

   string_list_free(str_list);

   switch (hash_label)
   {
      case MENU_LABEL_CORE_LIST:
         {
            const char *dir = NULL;

            menu_list_get_last_stack(menu_list, &dir, NULL, NULL, NULL);

            list_size = file_list_get_size(info->list);

            for (i = 0; i < list_size; i++)
            {
               unsigned type = 0;
               char core_path[PATH_MAX_LENGTH]    = {0};
               char display_name[PATH_MAX_LENGTH] = {0};
               const char *path                   = NULL;

               menu_list_get_at_offset(info->list, i, &path, NULL, &type, NULL);

               if (type != MENU_FILE_CORE)
                  continue;

               fill_pathname_join(core_path, dir, path, sizeof(core_path));

               if (global->core_info &&
                     core_info_list_get_display_name(global->core_info,
                        core_path, display_name, sizeof(display_name)))
                  menu_list_set_alt_at_offset(info->list, i, display_name);
            }
            *need_sort = true;
         }
         break;
   }

   return 0;
}

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type)
{
   size_t i, list_size;
   int ret                     = 0;
   bool need_sort              = false;
   bool need_refresh           = false;
   bool need_push              = false;
   rarch_setting_t    *setting = NULL;
   menu_handle_t       *menu   = menu_driver_get_ptr();
   menu_navigation_t    *nav   = menu_navigation_get_ptr();
   global_t          *global   = global_get_ptr();
   settings_t      *settings   = config_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   switch (type)
   {
      case DISPLAYLIST_NONE:
         break;
      case DISPLAYLIST_INFO:
         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         break;
      case DISPLAYLIST_GENERIC:
         menu_driver_list_cache(MENU_LIST_PLAIN, 0);

         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         menu_navigation_clear(nav, true);
         menu_entries_set_refresh();
         break;
      case DISPLAYLIST_HELP_SCREEN_LIST:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_CONTROLS),
               menu_hash_to_str(MENU_LABEL_HELP_CONTROLS),
               0, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE),
               menu_hash_to_str(MENU_LABEL_HELP_WHAT_IS_A_CORE),
               0, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_LOADING_CONTENT),
               menu_hash_to_str(MENU_LABEL_HELP_LOADING_CONTENT),
               0, 0, 0);
#ifdef HAVE_LIBRETRODB
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_SCANNING_CONTENT),
               menu_hash_to_str(MENU_LABEL_HELP_SCANNING_CONTENT),
               0, 0, 0);
#endif
#ifdef HAVE_OVERLAY
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD),
               menu_hash_to_str(MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD),
               0, 0, 0);
#endif
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               menu_hash_to_str(MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               0, 0, 0);
         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_HELP:
         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         menu->push_help_screen = false;
         menu_display_fb_set_dirty();
         break;
      case DISPLAYLIST_MAIN_MENU:
      case DISPLAYLIST_SETTINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_settings(menu, info, info->flags);
         need_push    = true;
         break;
      case DISPLAYLIST_SETTINGS_SUBGROUP:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_settings_in_subgroup(info);
         need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_horizontal_list(info);

         need_sort    = true;
         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
      case DISPLAYLIST_CONTENT_SETTINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_load_content_settings(info);

         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_INFORMATION_LIST:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_information_list(info);

         need_push    = true;
         need_refresh = true;
         break;
      case DISPLAYLIST_ADD_CONTENT_LIST:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_add_content_list(info);

         need_push    = true;
         need_refresh = true;
         break;
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_load_content_list(info);

         need_push    = true;
         need_refresh = true;
         break;
      case DISPLAYLIST_OPTIONS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_cheats(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_remappings(info);

         need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
#ifdef HAVE_SHADER_MANAGER
         menu_list_clear(info->list);
         {
            struct video_shader *shader = video_shader_driver_get_current_shader();
            if (!shader)
            {
               menu_list_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_NO_SHADER_PARAMETERS),
                     "", 0, 0, 0);
               ret = 0;
            }
            else
            {
               ret = deferred_push_video_shader_parameters_common(info, shader,
                     (type == DISPLAYLIST_SHADER_PARAMETERS)
                     ? MENU_SETTINGS_SHADER_PARAMETER_0 : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
                     );
            }

            need_push = true;
         }
#endif
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         menu_list_clear(info->list);
         menu_displaylist_push_perfcounter(info,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               perf_counters_libretro : perf_counters_rarch,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               perf_ptr_libretro : perf_ptr_rarch ,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN : 
               MENU_SETTINGS_PERF_COUNTERS_BEGIN);
         ret = 0;

         need_refresh = false;
         need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_ENTRY:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_database_entry(info);

         need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_QUERY:
         menu_list_clear(info->list);
         ret = menu_database_parse_query(info->list,
               info->path, (info->path_c[0] == '\0') ? NULL : info->path_c);
         strlcpy(info->path, info->path_b, sizeof(info->path));

         need_sort    = true;
         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_shader_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_CORE_CONTENT:
         menu_list_clear(info->list);
#ifdef HAVE_NETWORKING
         menu_list_clear(info->list);
         print_buf_lines(info->list, core_buf, core_len, MENU_FILE_DOWNLOAD_CORE_CONTENT);
         need_push    = true;
         need_refresh = true;
#endif
         break;
      case DISPLAYLIST_CORES_UPDATER:
         menu_list_clear(info->list);
#ifdef HAVE_NETWORKING
         menu_list_clear(info->list);
         print_buf_lines(info->list, core_buf, core_len, MENU_FILE_DOWNLOAD_CORE);
         need_push    = true;
         need_refresh = true;
#endif
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         menu_list_clear(info->list);
         menu_displaylist_realloc_settings(&menu->entries, SL_FLAG_ALL_SETTINGS);

         setting = menu_setting_find(menu_hash_to_str(MENU_LABEL_VALUE_DRIVER_SETTINGS));

         if (settings->menu.collapse_subgroups_enable)
         {
            for (; setting->type != ST_NONE; setting++)
            {
               if (setting->type == ST_GROUP)
               {
                  if (setting->flags & SD_FLAG_ADVANCED &&
                        !settings->menu.show_advanced_settings)
                     continue;
                  menu_list_push(info->list, setting->short_description,
                        setting->name, menu_setting_set_flags(setting), 0, 0);
               }
            }
         }
         else
         {
            for (; setting->type != ST_NONE; setting++)
            {
               char group_label[PATH_MAX_LENGTH]    = {0};
               char subgroup_label[PATH_MAX_LENGTH] = {0};

               if (setting->type == ST_GROUP)
                  strlcpy(group_label, setting->name, sizeof(group_label));
               else if (setting->type == ST_SUB_GROUP)
               {
                  char new_label[PATH_MAX_LENGTH] = {0};
                  char new_path[PATH_MAX_LENGTH]  = {0};

                  strlcpy(subgroup_label, setting->name, sizeof(group_label));
                  strlcpy(new_label, group_label, sizeof(new_label));
                  strlcat(new_label, "|", sizeof(new_label));
                  strlcat(new_label, subgroup_label, sizeof(new_label));

                  strlcpy(new_path, group_label, sizeof(new_path));
                  strlcat(new_path, " - ", sizeof(new_path));
                  strlcat(new_path, setting->short_description, sizeof(new_path));

                  menu_list_push(info->list, new_path,
                        new_label, MENU_SETTING_SUBGROUP, 0, 0);
               }
            }
         }

         need_push    = true;
         break;
      case DISPLAYLIST_PLAYLIST_COLLECTION:
         menu_list_clear(info->list);
         {
            char path_playlist[PATH_MAX_LENGTH] = {0};
            content_playlist_t *playlist        = NULL;

            if (menu->playlist)
               content_playlist_free(menu->playlist);

            fill_pathname_join(path_playlist,
                  settings->playlist_directory, info->path,
                  sizeof(path_playlist));
            menu->playlist  = content_playlist_init(path_playlist,
                  999);
            strlcpy(menu->db_playlist_file, path_playlist, sizeof(menu->db_playlist_file));
            strlcpy(path_playlist,
                  menu_hash_to_str(MENU_LABEL_COLLECTION), sizeof(path_playlist));
            playlist = menu->playlist;

            ret = menu_displaylist_parse_playlist(info, playlist, path_playlist, false);

            if (ret == 0)
            {
               need_sort    = true;
               need_refresh = true;
               need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_HISTORY:
         menu_list_clear(info->list);
         {
            char path_playlist[PATH_MAX_LENGTH] = {0};
            content_playlist_t *playlist        = g_defaults.history;

            strlcpy(path_playlist, "history", sizeof(path_playlist));

            ret = menu_displaylist_parse_playlist(info, playlist, path_playlist, true);

            if (ret == 0)
            {
               need_refresh = true;
               need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_INDEX),
               menu_hash_to_str(MENU_LABEL_DISK_INDEX),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0);
         menu_list_push(info->list, 
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS),
               menu_hash_to_str(MENU_LABEL_DISK_CYCLE_TRAY_STATUS),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_IMAGE_APPEND),
               menu_hash_to_str(MENU_LABEL_DISK_IMAGE_APPEND),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0);

         need_push    = true;
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_list_clear(info->list);
         menu_displaylist_parse_system_info(info);
         need_push    = true;
         need_refresh = true;
         break;
      case DISPLAYLIST_CORES_SUPPORTED:
      case DISPLAYLIST_CORES_COLLECTION_SUPPORTED:
         menu_list_clear(info->list);
         need_sort    = true;
         need_refresh = true;
         need_push    = true;

         {
            const core_info_t *core_info = NULL;
            core_info_list_get_supported_cores(global->core_info,
                  menu->deferred_path, &core_info, &list_size);

            if (list_size <= 0)
            {
               menu_list_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_NO_CORES_AVAILABLE),
                     "",
                     0, 0, 0);
            }
            else
            {
               for (i = 0; i < list_size; i++)
               {
                  if (type == DISPLAYLIST_CORES_COLLECTION_SUPPORTED)
                     menu_list_push(info->list, core_info[i].path, "",
                           MENU_FILE_CORE, 0, 0);
                  else
                     menu_list_push(info->list, core_info[i].path,
                           menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST_OK),
                           MENU_FILE_CORE, 0, 0);
                  menu_list_set_alt_at_offset(info->list, i,
                        core_info[i].display_name);
               }
            }
         }
         break;
      case DISPLAYLIST_CORE_INFO:
         menu_list_clear(info->list);
         menu_displaylist_parse_core_info(info);
         need_push = true;
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         menu_list_clear(info->list);
         if (system && system->core_options)
         {
            size_t opts = core_option_size(system->core_options);

            if (opts == 0)
            {
               menu_list_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), "",
                     MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
            }
            else
            {
               for (i = 0; i < opts; i++)
                  menu_list_push(info->list,
                        core_option_get_desc(system->core_options, i), "",
                        MENU_SETTINGS_CORE_OPTION_START + i, 0, 0);
            }
         }
         else
            menu_list_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), "",
                  MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
         need_push = true;
         break;
      case DISPLAYLIST_DEFAULT:
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_DATABASES:
      case DISPLAYLIST_DATABASE_CURSORS:
      case DISPLAYLIST_DATABASE_PLAYLISTS:
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_IMAGES:
      case DISPLAYLIST_OVERLAYS:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_RECORD_CONFIG_FILES:
      case DISPLAYLIST_CONFIG_FILES:
      case DISPLAYLIST_CONTENT_HISTORY:
         menu_list_clear(info->list);
         if (menu_displaylist_parse_generic(info, &need_sort) == 0)
         {
            need_refresh = true;
            need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL:
         menu_list_clear(info->list);
         menu_displaylist_parse_generic(info, &need_sort);
         break;
      case DISPLAYLIST_ARCHIVE_ACTION:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_OPEN_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_OPEN_ARCHIVE),
               0, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_LOAD_ARCHIVE),
               0, 0, 0);
         need_push = true;
         break;
      case DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_OPEN_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE),
               0, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_ARCHIVE),
               menu_hash_to_str(MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE),
               0, 0, 0);
         need_push = true;
         break;
   }

   if (need_sort)
      file_list_sort_on_alt(info->list);

   if (need_push)
   {
      driver_t *driver                = driver_get_ptr();
      const ui_companion_driver_t *ui = ui_companion_get_ptr();

      if (need_refresh)
         menu_list_refresh(info->list);
      menu_driver_populate_entries(info->path, info->label, info->type);

      if (ui && driver)
         ui->notify_list_loaded(driver->ui_companion_data,
               info->list, info->menu_list);
   }

   return ret;
}

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list)
{
   menu_file_list_cbs_t *cbs    = NULL;
   const char *path             = NULL;
   const char *label            = NULL;
   uint32_t          hash_label = 0;
   unsigned type                = 0;
   menu_displaylist_info_t info = {0};
   menu_entries_t *entries      = menu_entries_get_ptr();

   menu_list_get_last_stack(entries->menu_list, &path, &label, &type, NULL);

   info.list      = list;
   info.menu_list = menu_list;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   hash_label     = menu_hash_calculate(label);

   if (!info.list)
      return -1;

   switch (hash_label)
   {
      case MENU_VALUE_MAIN_MENU:
         info.flags = SL_FLAG_MAIN_MENU | SL_FLAG_MAIN_MENU_SETTINGS;
         return menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);
      case MENU_VALUE_HORIZONTAL_MENU:
         return menu_displaylist_push_list(&info, DISPLAYLIST_HORIZONTAL);
   }

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(entries->menu_list);

   if (cbs->action_deferred_push)
      return cbs->action_deferred_push(&info);

   return 0;
}

/**
 * menu_displaylist_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu display list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_displaylist_init(void *data)
{
   menu_handle_t       *menu    = (menu_handle_t*)data;
   menu_list_t       *menu_list = menu_list_get_ptr();
   menu_navigation_t       *nav = menu_navigation_get_ptr();
   menu_displaylist_info_t info = {0};
   if (!menu || !menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU | SL_FLAG_MAIN_MENU_SETTINGS;
   strlcpy(info.label, menu_hash_to_str(MENU_VALUE_MAIN_MENU), sizeof(info.label));

   menu_list_push(menu_list->menu_stack,
         info.path, info.label, info.type, info.flags, 0);
   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);
   menu_navigation_clear(nav, true);

   return true;
}

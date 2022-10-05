/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
 *  Copyright (C) 2022-2022 - Jahed Ahmed
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
#include <streams/file_stream.h>
#include <string/stdstring.h>
#if defined(RS90)
#include <lists/dir_list.h>
#endif

#include <stdlib.h>

#include "dingux_utils.h"

#define DINGUX_ALLOW_DOWNSCALING_FILE     "/sys/devices/platform/jz-lcd.0/allow_downscaling"
#define DINGUX_KEEP_ASPECT_RATIO_FILE     "/sys/devices/platform/jz-lcd.0/keep_aspect_ratio"
#define DINGUX_INTEGER_SCALING_FILE       "/sys/devices/platform/jz-lcd.0/integer_scaling"
#define DINGUX_SHARPNESS_UPSCALING_FILE   "/sys/devices/platform/jz-lcd.0/sharpness_upscaling"
#define DINGUX_SHARPNESS_DOWNSCALING_FILE "/sys/devices/platform/jz-lcd.0/sharpness_downscaling"
#define DINGUX_BATTERY_CAPACITY_FILE      "/sys/class/power_supply/battery/capacity"

/* Base path defines */
#define DINGUX_HOME_ENVAR                 "HOME"
#define DINGUX_BASE_DIR                   "retroarch"
#define DINGUX_BASE_DIR_HIDDEN            ".retroarch"
#define DINGUX_RS90_MEDIA_PATH            "/media"
#define DINGUX_RS90_DEFAULT_SD_PATH       "/media/mmcblk0p1"
#define DINGUX_RS90_DATA_PATH             "/media/data"

/* OpenDingux Beta defines */
#define DINGUX_BATTERY_VOLTAGE_MIN        "/sys/class/power_supply/jz-battery/voltage_min_design"
#define DINGUX_BATTERY_VOLTAGE_MAX        "/sys/class/power_supply/jz-battery/voltage_max_design"
#define DINGUX_BATTERY_VOLTAGE_NOW        "/sys/class/power_supply/jz-battery/voltage_now"
#define DINGUX_SCALING_MODE_ENVAR         "SDL_VIDEO_KMSDRM_SCALING_MODE"
#define DINGUX_SCALING_SHARPNESS_ENVAR    "SDL_VIDEO_KMSDRM_SCALING_SHARPNESS"
#define DINGUX_VIDEO_REFRESHRATE_ENVAR    "SDL_VIDEO_REFRESHRATE"

/* Miyoo defines */
#define MIYOO_BATTERY_VOLTAGE_NOW_FILE    "/sys/class/power_supply/miyoo-battery/voltage_now"

/* RetroFW */
#define RETROFW_BATTERY_VOLTAGE_NOW_FILE "/proc/jz/battery"

/* Enables/disables downscaling when using
 * the IPU hardware scaler */
bool dingux_ipu_set_downscaling_enable(bool enable)
{
#if !defined(DINGUX_BETA)
   const char *path       = DINGUX_ALLOW_DOWNSCALING_FILE;
   const char *enable_str = enable ? "1" : "0";
   /* Check whether file exists */
   if (!path_is_valid(path))
      return false;
   /* Write enable state to file */
   if (!filestream_write_file(
            path, enable_str, 1))
      return false;
#endif
   return true;
}

/* Sets the video scaling mode when using the
 * IPU hardware scaler
 * - keep_aspect: When 'true', aspect ratio correction
 *   (1:1 PAR) is applied. When 'false', image is
 *   stretched to full screen dimensions
 * - integer_scale: When 'true', enables integer
 *   scaling. This implicitly sets keep_aspect to
 *   'true' (since integer scaling is by definition
 *   1:1 PAR)
 * Note: OpenDingux stock firmware allows keep_aspect
 * and integer_scale to be set independently, hence
 * separate boolean values. OpenDingux beta properly
 * groups the settings into a single scaling type
 * parameter. When supporting both firmwares, it would
 * be cleaner to refactor this function to accept one
 * enum rather than 2 booleans - but this would break
 * users' existing configs, so we maintain the old
 * format... */
bool dingux_ipu_set_scaling_mode(bool keep_aspect, bool integer_scale)
{
#if defined(DINGUX_BETA)
   const char *scaling_str = "0";

   /* integer_scale takes priority */
   if (integer_scale)
      scaling_str = "2";
   else if (keep_aspect)
      scaling_str = "1";

   return (setenv(DINGUX_SCALING_MODE_ENVAR, scaling_str, 1) == 0);
#else
   const char *keep_aspect_path   = DINGUX_KEEP_ASPECT_RATIO_FILE;
   const char *keep_aspect_str    = keep_aspect ? "1" : "0";
   bool keep_aspect_success       = false;

   const char *integer_scale_path = DINGUX_INTEGER_SCALING_FILE;
   const char *integer_scale_str  = integer_scale ? "1" : "0";
   bool integer_scale_success     = false;

   /* Set keep_aspect */
   if (path_is_valid(keep_aspect_path))
      keep_aspect_success = filestream_write_file(
         keep_aspect_path, keep_aspect_str, 1);

   /* Set integer_scale */
   if (path_is_valid(integer_scale_path))
      integer_scale_success = filestream_write_file(
         integer_scale_path, integer_scale_str, 1);

   return (keep_aspect_success && integer_scale_success);
#endif
}

/* Sets the image filtering method when
 * using the IPU hardware scaler */
bool dingux_ipu_set_filter_type(enum dingux_ipu_filter_type filter_type)
{
   /* Sharpness settings range is [0,32]
    * - 0:      nearest-neighbour
    * - 1:      bilinear
    * - 2...32: bicubic (translating to a sharpness
    *                    factor of -0.25..-4.0 internally)
    * Default bicubic sharpness factor is
    * (-0.125 * 8) = -1.0 */
#if !defined(DINGUX_BETA)
   const char *upscaling_path   = DINGUX_SHARPNESS_UPSCALING_FILE;
   const char *downscaling_path = DINGUX_SHARPNESS_DOWNSCALING_FILE;
   bool upscaling_success       = false;
   bool downscaling_success     = false;
#endif
   const char *sharpness_str    = "8";

   /* Check filter type */
   switch (filter_type)
   {
      case DINGUX_IPU_FILTER_BILINEAR:
         sharpness_str = "1";
         break;
      case DINGUX_IPU_FILTER_NEAREST:
         sharpness_str = "0";
         break;
      default:
         /* sharpness_str is already set to 8
          * by default */
         break;
   }

#if defined(DINGUX_BETA)
   return (setenv(DINGUX_SCALING_SHARPNESS_ENVAR, sharpness_str, 1) == 0);
#else
   /* Set upscaling sharpness */
   if (path_is_valid(upscaling_path))
      upscaling_success = filestream_write_file(
         upscaling_path, sharpness_str, 1);

   /* Set downscaling sharpness */
   if (path_is_valid(downscaling_path))
      downscaling_success = filestream_write_file(
         downscaling_path, sharpness_str, 1);

   return (upscaling_success && downscaling_success);
#endif
}

#if defined(DINGUX_BETA)
/* Sets the refresh rate of the integral LCD panel.
 * If specified value is invalid, will set refresh
 * rate to 60 Hz.
 * Returns a floating point representation of the
 * resultant hardware refresh rate. In the event
 * that a refresh rate cannot be set (i.e. hardware
 * error), returns 0.0 */
float dingux_set_video_refresh_rate(enum dingux_refresh_rate refresh_rate)
{
   float refresh_rate_float     = 60.0f;
   const char *refresh_rate_str = "60";

   /* Check filter type */
   switch (refresh_rate)
   {
      case DINGUX_REFRESH_RATE_50HZ:
         refresh_rate_float     = 50.0f;
         refresh_rate_str       = "50";
         break;
      default:
         /* Refresh rate is already set to 60 Hz
          * by default */
         break;
   }

   if (setenv(DINGUX_VIDEO_REFRESHRATE_ENVAR, refresh_rate_str, 1) != 0)
      return 0.0f;

   return refresh_rate_float;
}

/* Gets the currently set refresh rate of the
 * integral LCD panel. */
bool dingux_get_video_refresh_rate(enum dingux_refresh_rate *refresh_rate)
{
   const char *refresh_rate_str = getenv(DINGUX_VIDEO_REFRESHRATE_ENVAR);

   /* If environment variable is unset, refresh
    * rate defaults to 60 Hz */
   if (!refresh_rate_str)
   {
      *refresh_rate = DINGUX_REFRESH_RATE_60HZ;
      return true;
   }

   if (string_is_equal(refresh_rate_str, "60"))
      *refresh_rate = DINGUX_REFRESH_RATE_60HZ;
   else if (string_is_equal(refresh_rate_str, "50"))
      *refresh_rate = DINGUX_REFRESH_RATE_50HZ;
   else
      return false;

   return true;
}
#endif

/* Resets the IPU hardware scaler to the
 * default configuration */
bool dingux_ipu_reset(void)
{
#if defined(DINGUX_BETA)
   unsetenv(DINGUX_SCALING_MODE_ENVAR);
   unsetenv(DINGUX_SCALING_SHARPNESS_ENVAR);
   unsetenv(DINGUX_VIDEO_REFRESHRATE_ENVAR);
   return true;
#else
   return dingux_ipu_set_scaling_mode(true, false) &&
          dingux_ipu_set_filter_type(DINGUX_IPU_FILTER_BICUBIC);
#endif
}

#if defined(RETROFW)
static uint64_t read_battery_ignore_size(const char *path)
{
   int64_t file_len   = 0;
   char file_buf[20];
   int sys_file_value = 0;
   RFILE *file;

   /* Check whether file exists */
   if (!path_is_valid(path))
      return -1;

   memset(file_buf, 0, sizeof(file_buf));

   if (!(file = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return -1;

   file_len = filestream_read(file, file_buf, sizeof(file_buf) - 1);
   if (filestream_close(file) != 0)
      if (file)
         free(file);

   if (file_len <= 0)
      return -1;

   return strtoul(file_buf, NULL, 10);
}

int retrofw_get_battery_level(enum frontend_powerstate *state)
{
   /* retrofw battery only provides "voltage_now". Values are based on gmenu2x with some interpolation */
   uint32_t rawval = read_battery_ignore_size(RETROFW_BATTERY_VOLTAGE_NOW_FILE);
   int voltage_now = rawval & 0x7fffffff;
   if (voltage_now > 10000)
   {
      *state = FRONTEND_POWERSTATE_NONE;
      return -1;
   }
   if (rawval & 0x80000000)
   {
      *state = FRONTEND_POWERSTATE_CHARGING;
      if (voltage_now > 4000)
         *state = FRONTEND_POWERSTATE_CHARGED;
   }
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;
   if (voltage_now < 0)
      return -1; /* voltage_now not available */
   if (voltage_now > 4000)
      return 100;
   if (voltage_now > 3700)
      return 40 + (voltage_now - 3700) / 5;
   if (voltage_now > 3520)
      return 20 + (voltage_now - 3520) / 9;
   if (voltage_now > 3330)
      return 1 + (voltage_now - 3330) * 10;
   return 0;
}
#else
static int dingux_read_battery_sys_file(const char *path)
{
   int64_t file_len   = 0;
   char *file_buf     = NULL;
   int sys_file_value = 0;

   /* Check whether file exists */
   if (!path_is_valid(path))
      goto error;

   /* Read file */
   if (!filestream_read_file(path, (void**)&file_buf, &file_len) ||
       (file_len == 0) ||
       !file_buf)
      goto error;

   /* Convert to integer */
   sys_file_value = atoi(file_buf);
   free(file_buf);
   file_buf = NULL;

   return sys_file_value;

error:
   if (file_buf)
   {
      free(file_buf);
      file_buf = NULL;
   }

   return -1;
}

/* Fetches internal battery level */
int dingux_get_battery_level(void)
{
#if defined(DINGUX_BETA)
   /* Taken from https://github.com/OpenDingux/gmenu2x/blob/master/src/battery.cpp
    * No 'capacity' file in sysfs - Do a dumb approximation of the capacity
    * using the current voltage reported and the min/max voltages of the
    * battery */
   int voltage_min = 0;
   int voltage_max = 0;
   int voltage_now = 0;

   voltage_min = dingux_read_battery_sys_file(DINGUX_BATTERY_VOLTAGE_MIN);
   if (voltage_min < 0)
      return -1;

   voltage_max = dingux_read_battery_sys_file(DINGUX_BATTERY_VOLTAGE_MAX);
   if (voltage_max < 0)
      return -1;

   voltage_now = dingux_read_battery_sys_file(DINGUX_BATTERY_VOLTAGE_NOW);
   if (voltage_now < 0)
      return -1;

   if ((voltage_max <= voltage_min) ||
       (voltage_now <  voltage_min))
      return -1;

   return (int)(((voltage_now - voltage_min) * 100) / (voltage_max - voltage_min));
#elif defined(MIYOO)
   /* miyoo-battery only provides "voltage_now". Results are based on
    * value distribution while running a game at max load. */
   int voltage_now = dingux_read_battery_sys_file(MIYOO_BATTERY_VOLTAGE_NOW_FILE);
   if (voltage_now < 0)
      return -1;     /* voltage_now not available */
   if (voltage_now > 4300)
      return 100;    /* 4320 */
   if (voltage_now > 4200)
      return 90;     /* 4230 */
   if (voltage_now > 4100)
      return 80;     /* 4140 */
   if (voltage_now > 4000)
      return 70;     /* 4050 */
   if (voltage_now > 3900)
      return 60;     /* 3960 */
   if (voltage_now > 3800)
      return 50;     /* 3870 */
   if (voltage_now > 3700)
      return 40;     /* 3780 */
   if (voltage_now > 3600)
      return 30;     /* 3690 */
   if (voltage_now > 3550)
      return 20;     /* 3600 */
   if (voltage_now > 3500)
      return 10;     /* 3510 */
   if (voltage_now > 3400)
      return 5;      /* 3420 */
   if (voltage_now > 3300)
      return 1;      /* 3330 */
   return 0;         /* 3240 */
#else
   return dingux_read_battery_sys_file(DINGUX_BATTERY_CAPACITY_FILE);
#endif
}
#endif

/* Fetches the path of the base 'retroarch'
 * directory */
void dingux_get_base_path(char *path, size_t len)
{
   const char *home             = NULL;
#if defined(RS90)
   struct string_list *dir_list = NULL;
#endif

   if (!path || (len < 1))
      return;

#if defined(RS90)
   /* The RS-90 home directory is located on the
    * device's internal storage. This has limited
    * space (a total of only 256MB), such that it
    * is impractical to store cores and user files
    * here. We therefore attempt to use a base
    * path on the external microsd card */

   /* Get list of directories in /media */
   if ((dir_list = dir_list_new(DINGUX_RS90_MEDIA_PATH,
         NULL, true, true, false, false)))
   {
      size_t i;
      bool path_found = false;

      for (i = 0; i < dir_list->size; i++)
      {
         const char *dir_path = dir_list->elems[i].data;
         int dir_type         = dir_list->elems[i].attr.i;

         /* Skip files and invalid entries */
         if ((dir_type != RARCH_DIRECTORY) ||
             string_is_empty(dir_path) ||
             string_is_equal(dir_path, DINGUX_RS90_DATA_PATH))
            continue;

         /* Build 'retroarch' subdirectory path */
         snprintf(path, len, "%s%c%s", dir_path,
               PATH_DEFAULT_SLASH_C(), DINGUX_BASE_DIR);

         /* We can use this subdirectory path if:
          * - Directory corresponds to an unlabelled
          *   microsd card
          * - Subdirectory already exists */
         if (string_is_equal(dir_path, DINGUX_RS90_DEFAULT_SD_PATH) ||
             path_is_directory(path))
         {
            path_found = true;
            break;
         }
      }

      dir_list_free(dir_list);

      if (path_found)
         return;
   }
#endif
   /* Get home directory
    *
    * If a home directory is found (which should
    * always be the case), base path is "$HOME/.retroarch"
    * > If home path is unset, use existing UNIX frontend
    *   driver default of "retroarch" (this will ultimately
    *   fail, but there is nothing else we can do...) */
   if ((home = getenv(DINGUX_HOME_ENVAR)))
      snprintf(path, len, "%s%c%s", home,
            PATH_DEFAULT_SLASH_C(), DINGUX_BASE_DIR_HIDDEN);
   else
      strlcpy(path, DINGUX_BASE_DIR, len);
}

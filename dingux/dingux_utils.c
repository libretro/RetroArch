/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
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

#if defined(DINGUX_BETA)
#include <stdlib.h>
#endif

#include "dingux_utils.h"

#define DINGUX_ALLOW_DOWNSCALING_FILE     "/sys/devices/platform/jz-lcd.0/allow_downscaling"
#define DINGUX_KEEP_ASPECT_RATIO_FILE     "/sys/devices/platform/jz-lcd.0/keep_aspect_ratio"
#define DINGUX_INTEGER_SCALING_FILE       "/sys/devices/platform/jz-lcd.0/integer_scaling"
#define DINGUX_SHARPNESS_UPSCALING_FILE   "/sys/devices/platform/jz-lcd.0/sharpness_upscaling"
#define DINGUX_SHARPNESS_DOWNSCALING_FILE "/sys/devices/platform/jz-lcd.0/sharpness_downscaling"
#define DINGUX_BATTERY_CAPACITY_FILE      "/sys/class/power_supply/battery/capacity"

/* OpenDingux Beta defines */
#define DINGUX_BATTERY_VOLTAGE_MIN        "/sys/class/power_supply/jz-battery/voltage_min_design"
#define DINGUX_BATTERY_VOLTAGE_MAX        "/sys/class/power_supply/jz-battery/voltage_max_design"
#define DINGUX_BATTERY_VOLTAGE_NOW        "/sys/class/power_supply/jz-battery/voltage_now"
#define DINGUX_SCALING_MODE_ENVAR         "SDL_VIDEO_KMSDRM_SCALING_MODE"
#define DINGUX_SCALING_SHARPNESS_ENVAR    "SDL_VIDEO_KMSDRM_SCALING_SHARPNESS"

/* Enables/disables downscaling when using
 * the IPU hardware scaler */
bool dingux_ipu_set_downscaling_enable(bool enable)
{
#if defined(DINGUX_BETA)
   return true;
#else
   const char *path       = DINGUX_ALLOW_DOWNSCALING_FILE;
   const char *enable_str = enable ? "1" : "0";

   /* Check whether file exists */
   if (!path_is_valid(path))
      return false;

   /* Write enable state to file */
   return filestream_write_file(
         path, enable_str, 1);
#endif
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

/* Resets the IPU hardware scaler to the
 * default configuration */
bool dingux_ipu_reset(void)
{
#if defined(DINGUX_BETA)
   unsetenv(DINGUX_SCALING_MODE_ENVAR);
   unsetenv(DINGUX_SCALING_SHARPNESS_ENVAR);
   return true;
#else
   return dingux_ipu_set_scaling_mode(true, false) &&
          dingux_ipu_set_filter_type(DINGUX_IPU_FILTER_BICUBIC);
#endif
}

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
#else
   return dingux_read_battery_sys_file(DINGUX_BATTERY_CAPACITY_FILE);
#endif
}

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

#include "dingux_utils.h"

#define DINGUX_ALLOW_DOWNSCALING_FILE     "/sys/devices/platform/jz-lcd.0/allow_downscaling"
#define DINGUX_KEEP_ASPECT_RATIO_FILE     "/sys/devices/platform/jz-lcd.0/keep_aspect_ratio"
#define DINGUX_INTEGER_SCALING_FILE       "/sys/devices/platform/jz-lcd.0/integer_scaling"
#define DINGUX_SHARPNESS_UPSCALING_FILE   "/sys/devices/platform/jz-lcd.0/sharpness_upscaling"
#define DINGUX_SHARPNESS_DOWNSCALING_FILE "/sys/devices/platform/jz-lcd.0/sharpness_downscaling"
#define DINGUX_BATTERY_CAPACITY_FILE      "/sys/class/power_supply/battery/capacity"

/* Enables/disables downscaling when using
 * the IPU hardware scaler */
bool dingux_ipu_set_downscaling_enable(bool enable)
{
   const char *path       = DINGUX_ALLOW_DOWNSCALING_FILE;
   const char *enable_str = enable ? "1" : "0";

   /* Check whether file exists */
   if (!path_is_valid(path))
      return false;

   /* Write enable state to file */
   return filestream_write_file(
         path, enable_str, 1);
}

/* Enables/disables aspect ratio correction
 * (1:1 PAR) when using the IPU hardware
 * scaler (disabling this will stretch the
 * image to the full screen dimensions) */
bool dingux_ipu_set_aspect_ratio_enable(bool enable)
{
   const char *path       = DINGUX_KEEP_ASPECT_RATIO_FILE;
   const char *enable_str = enable ? "1" : "0";

   /* Check whether file exists */
   if (!path_is_valid(path))
      return false;

   /* Write enable state to file */
   return filestream_write_file(
         path, enable_str, 1);
}

/* Enables/disables integer scaling when
 * using the IPU hardware scaler */
bool dingux_ipu_set_integer_scaling_enable(bool enable)
{
   const char *path       = DINGUX_INTEGER_SCALING_FILE;
   const char *enable_str = enable ? "1" : "0";

   /* Check whether file exists */
   if (!path_is_valid(path))
      return false;

   /* Write enable state to file */
   return filestream_write_file(
         path, enable_str, 1);
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
   const char *upscaling_path   = DINGUX_SHARPNESS_UPSCALING_FILE;
   const char *downscaling_path = DINGUX_SHARPNESS_DOWNSCALING_FILE;
   const char *sharpness_str    = "8";
   bool upscaling_success       = false;
   bool downscaling_success     = false;

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

   /* Set upscaling sharpness */
   if (path_is_valid(upscaling_path))
      upscaling_success = filestream_write_file(
         upscaling_path, sharpness_str, 1);
   else
      upscaling_success = false;

   /* Set downscaling sharpness */
   if (path_is_valid(downscaling_path))
      downscaling_success = filestream_write_file(
         downscaling_path, sharpness_str, 1);
   else
      downscaling_success = false;

   return (upscaling_success && downscaling_success);
}

/* Fetches internal battery level */
int dingux_get_battery_level(void)
{
   const char *path  = DINGUX_BATTERY_CAPACITY_FILE;
   int64_t file_len  = 0;
   char *file_buf    = NULL;
   int battery_level = 0;

   /* Check whether file exists */
   if (!path_is_valid(path))
      goto error;

   /* Read file */
   if (!filestream_read_file(path, (void**)&file_buf, &file_len) ||
       (file_len == 0))
      goto error;

   /* Convert to integer */
   if (file_buf)
   {
      battery_level = atoi(file_buf);

      free(file_buf);
      file_buf = NULL;
   }

   return battery_level;

error:
   if (file_buf)
   {
      free(file_buf);
      file_buf = NULL;
   }

   return -1;
}

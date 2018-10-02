/* CRT SwitchRes Core
 * Copyright (C) 2018 Alphanu / Ben Templeman.
 *
 * RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string.h>
#include <stdlib.h>

#include "video_driver.h"
#include "video_crt_switch.h"
#include "video_display_server.h"

static unsigned ra_core_width     = 0;
static unsigned ra_core_height    = 0;
static unsigned ra_tmp_width      = 0;
static unsigned ra_tmp_height     = 0;
static unsigned ra_set_core_hz    = 0;
static unsigned orig_width        = 0;
static unsigned orig_height       = 0;
static int crt_center_adjust = 0;

static bool first_run             = true;

static float ra_tmp_core_hz       = 0.0f;
static float fly_aspect           = 0.0f;
static float ra_core_hz           = 0.0f;

static void crt_check_first_run(void)
{
   if (!first_run)
      return;

   first_run   = false;
}

static void switch_crt_hz(void)
{
   if (ra_core_hz == ra_tmp_core_hz)
      return;
   /* set hz float to an int for windows switching */
   if (ra_core_hz < 100)
   {
      if (ra_core_hz < 53)
         ra_set_core_hz = 50;
      if (ra_core_hz >= 53  &&  ra_core_hz < 57)
         ra_set_core_hz = 55;
      if (ra_core_hz >= 57)
         ra_set_core_hz = 60;
   }

   if (ra_core_hz > 100)
   {
      if (ra_core_hz < 106)
         ra_set_core_hz = 120;
      if (ra_core_hz >= 106  &&  ra_core_hz < 114)
         ra_set_core_hz = 110;
      if (ra_core_hz >= 114)
         ra_set_core_hz = 120;
   }

   video_monitor_set_refresh_rate(ra_set_core_hz);

   ra_tmp_core_hz = ra_core_hz;
}

void crt_aspect_ratio_switch(unsigned width, unsigned height)
{
   /* send aspect float to videeo_driver */
   fly_aspect = (float)width / height;
   video_driver_set_aspect_ratio_value((float)fly_aspect);
}

static void switch_res_crt(unsigned width, unsigned height)
{
   if (height > 100)
   {
      video_display_server_switch_resolution(width, height,
            ra_set_core_hz, ra_core_hz, crt_center_adjust);
      video_driver_apply_state_changes();
   }
}

/* Create correct aspect to fit video if resolution does not exist */
static void crt_screen_setup_aspect(unsigned width, unsigned height)
{

   switch_crt_hz();
   /* get original resolution of core */
   if (height == 4)
   {
      /* detect menu only */
      if (width < 1920)
         width = 320;

      height = 240;

      crt_aspect_ratio_switch(width, height);
   }

   if (height < 200 && height != 144)
   {
      crt_aspect_ratio_switch(width, height);
      height = 200;
   }

   if (height > 200)
      crt_aspect_ratio_switch(width, height);

   if (height == 144 && ra_set_core_hz == 50)
   {
      height = 288;
      crt_aspect_ratio_switch(width, height);
   }

   if (height > 200 && height < 224)
   {
      crt_aspect_ratio_switch(width, height);
      height = 224;
   }

   if (height > 224 && height < 240)
   {
      crt_aspect_ratio_switch(width, height);
      height = 240;
   }

   if (height > 240 && height < 255)
   {
      crt_aspect_ratio_switch(width, height);
      height = 254;
   }

   if (height == 528 && ra_set_core_hz == 60)
   {
      crt_aspect_ratio_switch(width, height);
      height = 480;
   }

   if (height >= 240 && height < 255 && ra_set_core_hz == 55)
   {
      crt_aspect_ratio_switch(width, height);
      height = 254;
   }

   switch_res_crt(width, height);
}

void crt_switch_res_core(unsigned width, unsigned height, float hz, unsigned crt_mode, int crt_switch_center_adjust)
{
   /* ra_core_hz float passed from within
    * void video_driver_monitor_adjust_system_rates(void) */
   ra_core_width  = width;
   ra_core_height = height;
   ra_core_hz     = hz;
   crt_center_adjust = crt_switch_center_adjust;

   if (crt_mode == 2)
   {
      if (hz > 53)
         ra_core_hz = hz * 2;

      if (hz <= 53)
         ra_core_hz = 120.0f;
   }

   crt_check_first_run();

   /* Detect resolution change and switch */
   if (
      (ra_tmp_height != ra_core_height) ||
      (ra_core_width != ra_tmp_width)
      )
      crt_screen_setup_aspect(width, height);

   ra_tmp_height  = ra_core_height;
   ra_tmp_width   = ra_core_width;

   /* Check if aspect is correct, if notchange */
   if (video_driver_get_aspect_ratio() != fly_aspect)
   {
      video_driver_set_aspect_ratio_value((float)fly_aspect);
      video_driver_apply_state_changes();
   }
}

void crt_video_restore(void)
{
   if (first_run)
      return;

   first_run = true;
}

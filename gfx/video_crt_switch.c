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

#if defined(_WIN32)
   #include <windows.h>
#endif


#include "video_driver.h"
#include "video_crt_switch.h"

static float ra_tmp_core_hz;
static int ra_core_width;
static int ra_core_height;
static int ra_tmp_width;
static int ra_tmp_height;
static float fly_aspect;
static float ra_core_hz;
static int ra_set_core_hz;
static int orig_width;			
static int orig_height;
static int first_run;

void crt_switch_res_core(int width, int height, float hz)
{
   /* ben_core_hz float passed from with in void video_driver_monitor_adjust_system_rates(void) */
   ra_core_width = width;		
   ra_core_height = height;
   ra_core_hz = hz;
   crt_check_first_run();
		
   if (ra_tmp_height != ra_core_height || ra_core_width != ra_tmp_width)
   { /* detect resolution change and switch */
      crt_screen_setup_aspect(width,height);
   }
   
   ra_tmp_height = ra_core_height;
   ra_tmp_width = ra_core_width;
		
   if (video_driver_get_aspect_ratio() != fly_aspect)
   {	/* check aspect is correct else change */
      video_driver_set_aspect_ratio_value((float)fly_aspect);
      crt_poke_video();
   }
}

void crt_check_first_run()
{		/* ruin of first boot to get current display resolution */
   if (first_run != 1)
   {
      #if defined(_WIN32)
         orig_height = GetSystemMetrics(SM_CYSCREEN);
         orig_width = GetSystemMetrics(SM_CXSCREEN);
      #endif

   }
   first_run = 1;
}


void crt_screen_setup_aspect(int width, int height)
{ /* create correct aspect to fit video if resolution does not exist */
   switch_crt_hz();
	/* get original resolution of core */	
	
	
   if (height == 4)
   {			
      if (width < 1920)
      {
         width = 640;
      }/* detect menu only */
         height = 480;
         crt_aspect_ratio_switch(width,height);
      }
      if (height < 191 && height != 144)
      {				
         crt_aspect_ratio_switch(width,height);
         height = 200;
      }	
      if (height > 191)
      {
         crt_aspect_ratio_switch(width,height);
      }
      if (height == 144 && ra_set_core_hz == 50)
      {				
         height = 288;
         crt_aspect_ratio_switch(width,height);
      }
      if (height > 200 && height < 224)
      {				
         crt_aspect_ratio_switch(width,height);
         height = 224;
      }
      if (height > 224 && height < 240)
      {				
         crt_aspect_ratio_switch(width,height);
         height = 240;
      }
      if (height > 240 && height < 255 )
      {								
         crt_aspect_ratio_switch(width,height);
         height = 254;
      }
      if (height == 528 && ra_set_core_hz == 60)
      {								
         crt_aspect_ratio_switch(width,height);
         height = 480;
      }
      if (height >= 240 && height < 255 && ra_set_core_hz == 55)
      {
         crt_aspect_ratio_switch(width,height);
         height = 254;
      }
		
   switch_res_crt(width, height);
}

void switch_res_crt(int width, int height){
		
   if ( height > 100)
   {
      
      crt_switch_res(width, height,0,ra_set_core_hz);
      crt_poke_video();
   }
}

void crt_aspect_ratio_switch(int width,int height)
{  /* send aspect float to videeo_driver */
   fly_aspect = (float)width/height;
   video_driver_set_aspect_ratio_value((float)fly_aspect);
}

void switch_crt_hz()
{  /* set hz float an int for windows switching */
	
   if (ra_core_hz != ra_tmp_core_hz)
   {
   if (ra_core_hz < 53 )
   {
      ra_set_core_hz = 50;	
   }
   if (ra_core_hz >= 53  &&  ra_core_hz < 57)
   {	
      ra_set_core_hz = 55;	
   }	
   if (ra_core_hz >= 57 )
   {	
      ra_set_core_hz = 60;	
   }	
   video_monitor_set_refresh_rate(ra_set_core_hz);
   ra_tmp_core_hz = ra_core_hz;
   }
}

void crt_video_restore()
{

   crt_switch_res(orig_width, orig_height,0,60);

}


void crt_switch_res(int width, int height, int f_restore,int  ra_hz)
{  /* windows function to swith resolutions */

   #if defined(_WIN32)
      DEVMODE curDevmode;
      DEVMODE devmode;
      DWORD flags = 0;
   
      int iModeNum;
      int depth = 0;
      int freq;
      LONG res;

      if (f_restore == 0)
      {
         freq = ra_hz;
      } 
      else 
      {
         freq = 0;
      }
	
      EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &curDevmode);
	
      if (width == curDevmode.dmPelsWidth)
      {
         width  = 0;							/* used to stop superresolution bug */
      }
      if (width == 0) 
      {
         width = curDevmode.dmPelsWidth;
      }
      if (height == 0) 
      {
         height = curDevmode.dmPelsHeight;
      }
      if (depth == 0) 
      {
         depth = curDevmode.dmBitsPerPel;
      }
      if (freq == 0) 
      {
         freq = curDevmode.dmDisplayFrequency;
      }

      for (iModeNum = 0; ; iModeNum++) 
      {
         if (EnumDisplaySettings(NULL, iModeNum, &devmode)) 
         {
		
            if (devmode.dmPelsWidth != width) 
            {
               continue;
            }

            if (devmode.dmPelsHeight != height) 
            {
               continue;
            }

            if (devmode.dmBitsPerPel != depth) 
            {
               continue;
            }

            if (devmode.dmDisplayFrequency != freq) 
            {
               continue;
            }

            devmode.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
            res = ChangeDisplaySettings(&devmode, CDS_TEST);

            switch (res) 
            {
            case DISP_CHANGE_SUCCESSFUL:
               res = ChangeDisplaySettings(&devmode, flags);
            switch (res) 
            {
            case DISP_CHANGE_SUCCESSFUL:
               return;

            case DISP_CHANGE_NOTUPDATED:
               return;

            default:
			
               break;
            }
               break;

            case DISP_CHANGE_RESTART:
		
               break;

            default:
			
               break;
            }
         }
         else 
         {
            break;
         }
      }
   #endif

   #if defined(linux)

   #endif   
}

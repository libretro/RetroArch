/* CRT SwitchRes Core 
 * Copyright (C) 2018 Ben Templeman.
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

#include <gfx/video_driver.h>

#include "video_crt_switch.h"
#include "video_crt_win.h"

static float ra_tmp_core_hz;
static int ra_core_width;
static int ra_core_height;
static int ra_tmp_width;
static int ra_tmp_height;
static float fly_aspect;

void switch_res_core(int width, int height, float hz)
{
	/* ben_core_hz float passed from with in void video_driver_monitor_adjust_system_rates(void) */
	ra_core_width = width;		
	ra_core_height = height;
	ra_core_hz = hz;
	check_first_run();
		
	if (ra_tmp_height != ra_core_height || ra_core_width != ra_tmp_width)
	{ /* detect resolution change and switch */
		screen_setup_aspect(width,height);
	}
		
	if (video_driver_get_aspect_ratio() != fly_aspect)
	{	/* check aspect is correct else change */
			video_driver_set_aspect_ratio_value((float)fly_aspect);
			crt_poke_video();
	}
}


void screen_setup_aspect(int width, int height)
{ /* create correct aspect to fit video if resolution does not exist */
	switch_crt_hz();
	/* get original resolution of core */	
	if (width >= 1900)
	{
		if (height == 4)
		{						/* detect menu only */
			height = 480;
			aspect_ratio_switch(width,height);
		}
		if (height < 191 && height != 144)
		{				
			aspect_ratio_switch(width,height);
			height = 200;
		}	
		if (height > 191)
		{
			aspect_ratio_switch(width,height);
		}
		if (height == 144 && ra_set_core_hz == 50)
		{				
			height = 288;
			aspect_ratio_switch(width,height);
		}
		if (height > 200 && height < 224)
		{				
			aspect_ratio_switch(width,height);
			height = 224;
		}
		if (height > 224 && height < 240)
		{				
			aspect_ratio_switch(width,height);
			height = 240;
		}
	
		if (height > 240 && height < 255 )
		{								
			aspect_ratio_switch(width,height);
			height = 254;
		}
		if (height == 528 && ra_set_core_hz == 60)
		{								
			aspect_ratio_switch(width,height);
			height = 480;
		}
	
		if (height >= 240 && height < 255 && ra_set_core_hz == 55)
		{
			aspect_ratio_switch(width,height);
			height = 254;
		}
		
	}
	switch_res_crt(width, height);
}

void switch_res_crt(int width, int height){
		
	if ( height > 100)
	{
			
	switch_res(width, height,0,ra_set_core_hz);
	crt_poke_video();
	ra_tmp_height = ra_core_height;
	ra_tmp_width = ra_core_width;
	
	}
	
}

void aspect_ratio_switch(int width,int height)
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
		video_monitor_set_refresh_rate(ra_core_hz);
		ra_tmp_core_hz = ra_core_hz;
	}
	
}



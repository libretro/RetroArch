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
#include <windows.h>
#include <stdlib.h>

#include "video_crt_switch.h"


void switch_res(int width, int height, int f_restore)
{  /* windows function to swith resolutions */
	
	DEVMODE curDevmode;
	DEVMODE devmode;
	DWORD flags = 0;
	int     iModeNum;
	int depth = 0;
	int freq;
	if (f_restore == 0)
	{
		freq = ra_set_core_hz;
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
			LONG res = ChangeDisplaySettings(&devmode, CDS_TEST);

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

}

void video_restore()
{
	switch_res(orig_width, orig_height,1);
}

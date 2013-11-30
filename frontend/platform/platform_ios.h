/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef _RARCH_PLATFORM_IOS_H
#define _RARCH_PLATFORM_IOS_H

#include <stdbool.h>
#include <file.h>

static struct
{
    bool portrait;
    bool portrait_upside_down;
    bool landscape_left;
    bool landscape_right;
    
    bool logging_enabled;
    
    char bluetooth_mode[64];
    
    struct
    {
        int stdout;
        int stderr;
        
        FILE* file;
    }  logging;
} apple_frontend_settings;

const void* apple_get_frontend_settings(void);
void ios_set_logging_state(const char *log_path, bool on);

extern bool apple_use_tv_mode;

#endif

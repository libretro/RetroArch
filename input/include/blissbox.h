/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __BLISSBOX_H
#define __BLISSBOX_H

#include <retro_common_api.h>

#define BLISSBOX_VID 0x16d0 /* requires firmware 2.0 */
#define BLISSBOX_PID 0x0d04 /* first of 4 controllers, each one increments PID by 1 */
#define BLISSBOX_UPDATE_MODE_PID 0x0a5f
#define BLISSBOX_OLD_PID 0x0a60
#define BLISSBOX_MAX_PADS 4
#define BLISSBOX_MAX_PAD_INDEX (BLISSBOX_MAX_PADS - 1)

#define BLISSBOX_USB_FEATURE_REPORT_ID 17

RETRO_BEGIN_DECLS

typedef struct
{
   const char *name;
   int index;
} blissbox_pad_type_t;

RETRO_END_DECLS

#endif

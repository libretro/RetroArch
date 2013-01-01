/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2013 - Hans-Kristian Arntzen
* Copyright (C) 2011-2013 - Daniel De Matteis
*
* RetroArch is free software: you can redistribute it and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Found-
* ation, either version 3 of the License, or (at your option) any later version.
*
* RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with RetroArch.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _XBOX_IO_SUPPORT_H
#define _XBOX_IO_SUPPORT_H

#ifdef _XBOX
#include <xtl.h>

HRESULT xbox_io_mount(char *szdrive, char *szdevice);
HRESULT xbox_io_unmount(char *szdrive, char *szdevice);
HRESULT xbox_io_remount(char *szdrive, char *szdevice);
HRESULT xbox_io_remap(char *szmapping);
HRESULT xbox_io_shutdown(void);
#endif

#endif

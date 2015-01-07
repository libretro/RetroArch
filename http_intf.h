/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
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

#ifndef _HTTP_INTF_H
#define _HTTP_INTF_H

#include "netplay_compat.h"
#include "http_lib.h"

enum
{
   HTTP_INTF_ERROR = 0,
   HTTP_INTF_PUT,
   HTTP_INTF_GET,
   HTTP_INTF_DELETE,
   HTTP_INTF_HEAD
};

int http_intf_command(unsigned mode, char *url);

#endif

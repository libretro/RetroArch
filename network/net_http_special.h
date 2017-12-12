/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2015-2017 - Andre Leiradella
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

#ifndef __NET_HTTP_SPECIAL_H
#define __NET_HTTP_SPECIAL_H

#include <libretro.h>

enum
{
   NET_HTTP_GET_OK = 0,
   NET_HTTP_GET_MALFORMED_URL,
   NET_HTTP_GET_CONNECT_ERROR,
   NET_HTTP_GET_TIMEOUT
};

int net_http_get(const char **result, size_t *size,
      const char *url, retro_time_t *timeout);

#endif

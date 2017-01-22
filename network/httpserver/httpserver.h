/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_HTTPSERVR_H
#define __RARCH_HTTPSERVR_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

int  httpserver_init(unsigned port);

void httpserver_destroy(void);

RETRO_END_DECLS

#endif /* __RARCH_HTTPSERVR_H */

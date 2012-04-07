/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_LOGGER_OVERRIDE_H
#define __SSNES_LOGGER_OVERRIDE_H

#if (defined(__CELLOS_LV2__) || defined(HW_RVL)) && defined(HAVE_LOGGER)

#include "console/logger/logger.h"

#define SSNES_LOG(...) do { \
   if (g_extern.verbose) logger_send("SSNES: " __VA_ARGS__); \
} while(0)

#define SSNES_ERR(...) do { \
   logger_send("SSNES [ERROR] :: " __VA_ARGS__); \
} while(0)

#define SSNES_WARN(...) do { \
   logger_send("SSNES [WARN] :: " __VA_ARGS__); \
} while(0)

#endif

#endif

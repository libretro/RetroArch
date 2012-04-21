/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_LOGGER_OVERRIDE_H
#define __SSNES_LOGGER_OVERRIDE_H

#if defined(HAVE_LOGGER)
#include "console/logger/logger.h"

#define SSNES_LOG(...) do { \
   if (g_extern.verbose) logger_send("SSNES: " __VA_ARGS__); \
} while(0)

#define SSNES_ERR(...) do { \
   logger_send("RetroArch [ERROR] :: " __VA_ARGS__); \
} while(0)

#define SSNES_WARN(...) do { \
   logger_send("RetroArch [WARN] :: " __VA_ARGS__); \
} while(0)
#elif defined(HAVE_FILE_LOGGER)
extern FILE * log_fp;
#ifndef SSNES_LOG
#define SSNES_LOG(...) do { \
   if (g_extern.verbose) \
      fprintf(log_fp, "SSNES: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef SSNES_ERR
#define SSNES_ERR(...) do { \
      fprintf(log_fp, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef SSNES_WARN
#define SSNES_WARN(...) do { \
      fprintf(log_fp, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#endif

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_UTIL_H
#define __RARCH_CHEEVOS_UTIL_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/*****************************************************************************
Setup - mainly for debugging
*****************************************************************************/

/* Define this macro to get extra-verbose log for cheevos. */
#define CHEEVOS_VERBOSE

/*****************************************************************************
End of setup
*****************************************************************************/

#define RCHEEVOS_TAG "[RCHEEVOS]: "
#define CHEEVOS_FREE(p) do { void* q = (void*)p; if (q != NULL) free(q); } while (0)

#ifdef CHEEVOS_VERBOSE

#define CHEEVOS_LOG RARCH_LOG
#define CHEEVOS_ERR RARCH_ERR

#else

#define CHEEVOS_LOG rcheevos_log
#define CHEEVOS_ERR RARCH_ERR

void rcheevos_log(const char *fmt, ...);

#endif

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_UTIL_H */

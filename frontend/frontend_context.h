/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef __FRONTEND_CONTEXT_H
#define __FRONTEND_CONTEXT_H

#include "../boolean.h"
#include "../driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_RGUI)
#include "menu/rgui.h"
#elif defined(HAVE_RMENU)
#include "menu/rmenu.h"
#endif

typedef struct frontend_ctx_driver
{
   void (*environment_get)(int argc, char *argv[], void *args);

   void (*init)(void *data);
   void (*deinit)(void *data);
   void (*exitspawn)(void);

   int (*process_args)(int argc, char *argv[], void *args);
   int (*process_events)(void *data);
   void (*exec)(const char *, bool);
   void (*shutdown)(bool);

   // Human readable string.
   const char *ident;
} frontend_ctx_driver_t;

extern const frontend_ctx_driver_t frontend_ctx_gx;
extern const frontend_ctx_driver_t frontend_ctx_ps3;
extern const frontend_ctx_driver_t frontend_ctx_xdk;
extern const frontend_ctx_driver_t frontend_ctx_qnx;
extern const frontend_ctx_driver_t frontend_ctx_apple;
extern const frontend_ctx_driver_t frontend_ctx_android;

const frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident); // Finds driver with ident. Does not initialize.
const frontend_ctx_driver_t *frontend_ctx_init_first(void); // Finds first suitable driver and initializes.

#endif

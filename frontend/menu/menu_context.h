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

#ifndef __MENU_CONTEXT_H
#define __MENU_CONTEXT_H

#include "../../boolean.h"
#include "../../driver.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "menu_common.h"

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void*, bool);
   void  (*render_messagebox)(void*, const char*);
   void  (*render)(void*);
   void* (*init)(void);
   void  (*free)(void*);
   void  (*init_assets)(void*);
   void  (*free_assets)(void*);

   // Human readable string.
   const char *ident;
} menu_ctx_driver_t;

extern const menu_ctx_driver_t menu_ctx_rmenu;
extern const menu_ctx_driver_t menu_ctx_rmenu_xui;
extern const menu_ctx_driver_t menu_ctx_rgui;

const menu_ctx_driver_t *menu_ctx_find_driver(const char *ident); // Finds driver with ident. Does not initialize.
bool menu_ctx_init_first(const menu_ctx_driver_t **driver, void **handle); // Finds first suitable driver and initializes.

#endif

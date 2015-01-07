/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef DRIVER_MENU_DISPLAY_H__
#define DRIVER_MENU_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void*);
   void  (*render_messagebox)(const char*);
   void  (*render)(void);
   void  (*frame)(void);
   void* (*init)(void);
   bool  (*init_lists)(void*);
   void  (*free)(void*);
   void  (*context_reset)(void*);
   void  (*context_destroy)(void*);
   void  (*populate_entries)(void*, const char *, const char *,
         unsigned);
   void  (*iterate)(void*, unsigned);
   int   (*input_postprocess)(uint64_t, uint64_t);
   void  (*navigation_clear)(void *, bool);
   void  (*navigation_decrement)(void *);
   void  (*navigation_increment)(void *);
   void  (*navigation_set)(void *, bool);
   void  (*navigation_set_last)(void *);
   void  (*navigation_descend_alphabet)(void *, size_t *);
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   void  (*list_insert)(void *, const char *, const char *, size_t);
   void  (*list_delete)(void *, size_t, size_t);
   void  (*list_clear)(void *);
   void  (*list_cache)(bool, unsigned);
   void  (*list_set_selection)(void *);
   void  (*init_core_info)(void *);
   void  (*update_core_info)(void *);

   const menu_ctx_driver_backend_t *backend;
   const char *ident;
} menu_ctx_driver_t;

#ifdef __cplusplus
}
#endif

#endif

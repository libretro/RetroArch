/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _MENU_NAVIGATION_H
#define _MENU_NAVIGATION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_navigation
{
   struct
   {
      /* Quick jumping indices with L/R.
       * Rebuilt when parsing directory. */
      struct
      {
         size_t list[2 * (26 + 2) + 1];
         unsigned size;
      } indices;
      unsigned acceleration;
   } scroll;
   size_t selection_ptr;
} menu_navigation_t;

menu_navigation_t *menu_navigation_get_ptr(void);

/**
 * menu_navigation_clear:
 * @pending_push          : pending push ?
 *
 * Clears the navigation pointer.
 **/
void menu_navigation_clear(menu_navigation_t *nav, bool pending_push);

/**
 * menu_navigation_decrement:
 *
 * Decrement the navigation pointer.
 **/
void menu_navigation_decrement(menu_navigation_t *nav, unsigned scroll_speed);

/**
 * menu_navigation_increment:
 *
 * Increment the navigation pointer.
 **/
void menu_navigation_increment(menu_navigation_t *nav, unsigned scroll_speed);

/**
 * menu_navigation_set:      
 * @idx                   : index to set navigation pointer to.
 * @scroll                : should we scroll when needed?
 *
 * Sets navigation pointer to index @idx.
 **/
void menu_navigation_set(menu_navigation_t *nav, size_t i, bool scroll);

/**
 * menu_navigation_set_last:
 *
 * Sets navigation pointer to last index.
 **/
void menu_navigation_set_last(menu_navigation_t *nav);

/**
 * menu_navigation_descend_alphabet:
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Descends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Beta',
 * navigation pointer will be set to an entry called 'Alpha'.
 **/
void menu_navigation_descend_alphabet(menu_navigation_t *nav, size_t *ptr_out);

/**
 * menu_navigation_ascends_alphabet:
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Ascends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Alpha',
 * navigation pointer will be set to an entry called 'Beta'.
 **/
void menu_navigation_ascend_alphabet(menu_navigation_t *nav, size_t *ptr_out);

ssize_t menu_navigation_get_current_selection(void);

#ifdef __cplusplus
}
#endif

#endif

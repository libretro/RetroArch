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

#ifndef __OSK_DRIVER__H
#define __OSK_DRIVER__H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_osk_driver
{
   void *(*init)(size_t size);
   void (*free)(void *data);
   bool (*enable_key_layout)(void *data);
   void (*oskutil_create_activation_parameters)(void *data);
   void (*write_msg)(void *data, const void *msg);
   void (*write_initial_msg)(void *data, const void *msg);
   bool (*start)(void *data);
   void (*lifecycle)(void *data, uint64_t status);
   void *(*get_text_buf)(void *data);
   const char *ident;
} input_osk_driver_t;

extern input_osk_driver_t input_ps3_osk;
extern input_osk_driver_t input_null_osk;

/**
 * osk_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to OSK driver at index. Can be NULL
 * if nothing found.
 **/
const void *osk_driver_find_handle(int index);

/**
 * osk_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of OSK driver at index. Can be NULL
 * if nothing found.
 **/
const char *osk_driver_find_ident(int index);

/**
 * config_get_osk_driver_options:
 *
 * Get an enumerated list of all OSK (onscreen keyboard) driver names,
 * separated by '|'.
 *
 * Returns: string listing of all OSK (onscreen keyboard) driver names,
 * separated by '|'.
 **/
const char* config_get_osk_driver_options(void);

/**
 * find_osk_driver:
 *
 * Find OSK (onscreen keyboard) driver.
 **/
void find_osk_driver(void);

void init_osk(void);

void uninit_osk(void);

#ifdef __cplusplus
}
#endif

#endif

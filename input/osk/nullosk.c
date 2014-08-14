/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>
#include "../../driver.h"


static void *nullosk_init(size_t size)
{
   return (void*)-1;
}

static void nullosk_free(void *data)
{
}

static bool nullosk_enable_key_layout(void *data)
{
   return true;
}

static void nullosk_create_activation_parameters(void *data)
{
}

static void nullosk_write_message(void *data, const void *data_msg)
{
}

static void nullosk_write_initial_message(void *data, const void *data_msg)
{
}

static bool nullosk_start(void *data) 
{
   return true;
}

static void *nullosk_get_text_buf(void *data)
{
   return NULL;
}

static void nullosk_lifecycle(void *data, uint64_t status)
{
}

const input_osk_driver_t input_null_osk = {
   nullosk_init,
   nullosk_free,
   nullosk_enable_key_layout,
   nullosk_create_activation_parameters,
   nullosk_write_message,
   nullosk_write_initial_message,
   nullosk_start,
   nullosk_lifecycle,
   nullosk_get_text_buf,
   "null"
};

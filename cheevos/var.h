/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_VAR_H
#define __RARCH_CHEEVOS_VAR_H

#include <stdint.h>

#include "cheevos.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef enum
{
   CHEEVOS_VAR_SIZE_BIT_0 = 0,
   CHEEVOS_VAR_SIZE_BIT_1,
   CHEEVOS_VAR_SIZE_BIT_2,
   CHEEVOS_VAR_SIZE_BIT_3,
   CHEEVOS_VAR_SIZE_BIT_4,
   CHEEVOS_VAR_SIZE_BIT_5,
   CHEEVOS_VAR_SIZE_BIT_6,
   CHEEVOS_VAR_SIZE_BIT_7,
   CHEEVOS_VAR_SIZE_NIBBLE_LOWER,
   CHEEVOS_VAR_SIZE_NIBBLE_UPPER,
   /* Byte, */
   CHEEVOS_VAR_SIZE_EIGHT_BITS, /* =Byte, */
   CHEEVOS_VAR_SIZE_SIXTEEN_BITS,
   CHEEVOS_VAR_SIZE_THIRTYTWO_BITS
} cheevos_var_size_t;

typedef enum
{
   /* compare to the value of a live address in RAM */
   CHEEVOS_VAR_TYPE_ADDRESS = 0,

   /* a number. assume 32 bit */
   CHEEVOS_VAR_TYPE_VALUE_COMP,

   /* the value last known at this address. */
   CHEEVOS_VAR_TYPE_DELTA_MEM,

   /* a custom user-set variable */
   CHEEVOS_VAR_TYPE_DYNAMIC_VAR
} cheevos_var_type_t;

typedef struct
{
   cheevos_var_size_t size;
   cheevos_var_type_t type;
   int                bank_id;
   bool               is_bcd;
   unsigned           value;
   unsigned           previous;
} cheevos_var_t;

void cheevos_var_parse(cheevos_var_t* var, const char** memaddr);
void cheevos_var_patch_addr(cheevos_var_t* var, cheevos_console_t console);

uint8_t* cheevos_var_get_memory(const cheevos_var_t* var);
unsigned cheevos_var_get_value(cheevos_var_t* var);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_VAR_H */

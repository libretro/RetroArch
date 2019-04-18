/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_COND_H
#define __RARCH_CHEEVOS_COND_H

#include "var.h"

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef enum
{
   CHEEVOS_COND_TYPE_STANDARD,
   CHEEVOS_COND_TYPE_PAUSE_IF,
   CHEEVOS_COND_TYPE_RESET_IF,
   CHEEVOS_COND_TYPE_ADD_SOURCE,
   CHEEVOS_COND_TYPE_SUB_SOURCE,
   CHEEVOS_COND_TYPE_ADD_HITS
} cheevos_cond_type_t;

typedef enum
{
   CHEEVOS_COND_OP_EQUALS,
   CHEEVOS_COND_OP_LESS_THAN,
   CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL,
   CHEEVOS_COND_OP_GREATER_THAN,
   CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL,
   CHEEVOS_COND_OP_NOT_EQUAL_TO
} cheevos_cond_op_t;

typedef struct
{
   cheevos_cond_type_t type;
   unsigned            req_hits;
   unsigned            curr_hits;
   char                pause;

   cheevos_var_t       source;
   cheevos_cond_op_t   op;
   cheevos_var_t       target;
} cheevos_cond_t;

void     cheevos_cond_parse(cheevos_cond_t* cond, const char** memaddr);
unsigned cheevos_cond_count_in_set(const char* memaddr, unsigned which);
void     cheevos_cond_parse_in_set(cheevos_cond_t* cond, const char* memaddr, unsigned which);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_COND_H */

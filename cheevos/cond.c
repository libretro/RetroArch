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

#include "cond.h"
#include "var.h"

#include "../retroarch.h"
#include "../verbosity.h"

/*****************************************************************************
Parsing
*****************************************************************************/

static cheevos_cond_op_t cheevos_cond_parse_operator(const char** memaddr)
{
   const char *str = *memaddr;
   cheevos_cond_op_t op;

   if (*str == '=' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str += 2;
   }
   else if (*str == '=')
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str++;
   }
   else if (*str == '!' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_NOT_EQUAL_TO;
      str += 2;
   }
   else if (*str == '<' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL;
      str += 2;
   }
   else if (*str == '<')
   {
      op = CHEEVOS_COND_OP_LESS_THAN;
      str++;
   }
   else if (*str == '>' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL;
      str += 2;
   }
   else if (*str == '>')
   {
      op = CHEEVOS_COND_OP_GREATER_THAN;
      str++;
   }
   else
   {
      CHEEVOS_ERR(CHEEVOS_TAG "unknown operator %c\n.", *str);
      op = CHEEVOS_COND_OP_EQUALS;
   }

   *memaddr = str;
   return op;
}

void cheevos_cond_parse(cheevos_cond_t* cond, const char** memaddr)
{
   const char* str = *memaddr;
   cond->type = CHEEVOS_COND_TYPE_STANDARD;

   if (*str != 0 && str[1] == ':')
   {
      int skip = 2;

      switch (*str)
      {
      case 'R':
         cond->type = CHEEVOS_COND_TYPE_RESET_IF;
         break;
      case 'P':
         cond->type = CHEEVOS_COND_TYPE_PAUSE_IF;
         break;
      case 'A':
         cond->type = CHEEVOS_COND_TYPE_ADD_SOURCE;
         break;
      case 'B':
         cond->type = CHEEVOS_COND_TYPE_SUB_SOURCE;
         break;
      case 'C':
         cond->type = CHEEVOS_COND_TYPE_ADD_HITS;
         break;
      default:
         skip = 0;
         break;
      }

      str += skip;
   }

   cheevos_var_parse(&cond->source, &str);
   cond->op = cheevos_cond_parse_operator(&str);
   cheevos_var_parse(&cond->target, &str);
   cond->curr_hits = 0;

   if (*str == '(' || *str == '.')
   {
      char* end;
      cond->req_hits = (unsigned)strtol(str + 1, &end, 10);
      str = end + (*end == ')' || *end == '.');
   }
   else
      cond->req_hits = 0;

   *memaddr = str;
}

unsigned cheevos_cond_count_in_set(const char* memaddr, unsigned which)
{
   cheevos_cond_t dummy;
   unsigned index = 0;
   unsigned count = 0;

   for (;;)
   {
      for (;;)
      {
         cheevos_cond_parse(&dummy, &memaddr);

         if (index == which)
            count++;

         if (*memaddr != '_')
            break;

         memaddr++;
      }

      index++;

      if (*memaddr != 'S')
         break;

      memaddr++;
   }

   return count;
}

void cheevos_cond_parse_in_set(cheevos_cond_t* cond, const char* memaddr, unsigned which)
{
   cheevos_cond_t dummy;
   unsigned index = 0;

   for (;;)
   {
      for (;;)
      {
         if (index == which)
         {
            cheevos_cond_parse(cond, &memaddr);
            cond++;
         }
         else
            cheevos_cond_parse(&dummy, &memaddr);

         if (*memaddr != '_')
            break;

         memaddr++;
      }

      index++;

      if (*memaddr != 'S')
         break;

      memaddr++;
   }
}

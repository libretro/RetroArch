/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2015-2017 - Andres Suarez
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include "settings.h"
#include "verbosity.h"
#include <string.h>

#include <compat/strl.h>
#include <compat/posix_string.h>

void setting_add(struct setting* set, void *ptr, size_t size, const char* name, enum set_type type, unsigned label, unsigned sublabel, unsigned description, 
   void *min, void *max, void *step, void* def)
{
   strlcpy(set->name, name, sizeof(set->name));
   switch (type)
   {
      case SET_INT:
      {
         int *aux;
         int *val;

         set->data = ptr;
         set->size = sizeof(int);
         set->label = label;
         set->sublabel = sublabel;
         set->description = description;

         set->min = (int *)calloc(1, sizeof(int));
         aux = (int*) set->min;
         val = (int*) min;
         *aux = *val;

         set->max = (int *)calloc(1, sizeof(int));
         aux = (int*) set->max;
         val = (int*) max;
         *aux = *val;

         set->step = (int *)calloc(1, sizeof(int));
         aux = (int*) set->step;
         val = (int*) step;
         *aux = *val;

         set->def = (int *)calloc(1, sizeof(int));
         aux = (int*) set->def;
         val = (int*) def;
         *aux = *val;
         break;
      }
      default:
         break;
   }
}

void setting_print(struct setting* set)
{
   switch(set->type)
   {
      case SET_INT:
      {
         int *min  = (int*)set->min;
         int *max  = (int*)set->max;
         int *step = (int*)set->step;
         int *val  = (int*)set->data;
         int *def  = (int*)set->def;
         RARCH_LOG("[setting] name:%s\ntype: int\nval: %d\nmin|max|step|def=%d|%d|%d|%d\n", set->name, *val, *min, *max, *step, *def);
         break;
      }
      default:
         break;
   }
}


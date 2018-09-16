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

#ifndef SETTINGS_H
#define SETTINGS_H

enum set_type
{
   SET_INT = 0,
   SET_UINT
};


struct setting
{
   /* pointer to the actual settings->entry */
   void *data;
   size_t size;
   /* setting type */
   enum set_type type;
   /* setting name so it can be referenced easily*/
   char name[100];
   /* reference to the label localization string */
   unsigned label;
   /* reference to the sublabel localization string */
   unsigned sublabel;
   /* reference to the description localization string */
   unsigned description;

   void *min;
   void *max;
   void *def;
   void *step;
};

void setting_add(struct setting* set, void *ptr, size_t size, const char* name, enum set_type type, unsigned label, unsigned sublabel, unsigned description, 
   void *min, void *max, void *step, void* def);

void setting_print(struct setting* set);

#endif
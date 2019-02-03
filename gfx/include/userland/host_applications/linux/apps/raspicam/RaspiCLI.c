/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, James Hughes
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * \file RaspiCLI.c
 * Code for handling command line parameters
 *
 * \date 4th March 2013
 * \Author: James Hughes
 *
 * Description
 *
 * Some functions/structures for command line parameter parsing
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "interface/vcos/vcos.h"

#include "RaspiCLI.h"

/**
 * Convert a string from command line to a comand_id from the list
 *
 * @param commands Array of command to check
 * @param num_command Number of commands in the array
 * @param arg String to search for in the list
 * @param num_parameters Returns the number of parameters used by the command
 *
 * @return command ID if found, -1 if not found
 *
 */
int raspicli_get_command_id(const COMMAND_LIST *commands, const int num_commands, const char *arg, int *num_parameters)
{
   int command_id = -1;
   int j;

   vcos_assert(commands);
   vcos_assert(num_parameters);
   vcos_assert(arg);

   if (!commands || !num_parameters || !arg)
      return -1;

   for (j = 0; j < num_commands; j++)
   {
      if (!strcmp(arg, commands[j].command) ||
            !strcmp(arg, commands[j].abbrev))
      {
         // match
         command_id = commands[j].id;
         *num_parameters = commands[j].num_parameters;
         break;
      }
   }

   return command_id;
}

/**
 * Display the list of commands in help format
 *
 * @param commands Array of command to check
 * @param num_command Number of commands in the arry
 *
 *
 */
void raspicli_display_help(const COMMAND_LIST *commands, const int num_commands)
{
   int i;

   vcos_assert(commands);

   if (!commands)
      return;

   for (i = 0; i < num_commands; i++)
   {
      fprintf(stdout, "-%s, -%s\t: %s\n", commands[i].abbrev,
              commands[i].command, commands[i].help);
   }
}

/**
 * Function to take a string, a mapping, and return the int equivalent
 * @param str Incoming string to match
 * @param map Mapping data
 * @param num_refs The number of items in the mapping data
 * @return The integer match for the string, or -1 if no match
 */
int raspicli_map_xref(const char *str, const XREF_T *map, int num_refs)
{
   int i;

   for (i=0; i<num_refs; i++)
   {
      if (!strcasecmp(str, map[i].mode))
      {
         return map[i].mmal_mode;
      }
   }
   return -1;
}

/**
 * Function to take a mmal enum (as int) and return the string equivalent
 * @param en Incoming int to match
 * @param map Mapping data
 * @param num_refs The number of items in the mapping data
 * @return const pointer to string, or NULL if no match
 */
const char *raspicli_unmap_xref(const int en, XREF_T *map, int num_refs)
{
   int i;

   for (i=0; i<num_refs; i++)
   {
      if (en == map[i].mmal_mode)
      {
         return map[i].mode;
      }
   }
   return NULL;
}

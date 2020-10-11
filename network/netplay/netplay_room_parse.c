/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <formats/rjson.h>
#include "netplay_discovery.h"
#include "../../verbosity.h"

enum netplay_parse_state
{
   STATE_START = 0,
   STATE_ARRAY_START,
   STATE_OBJECT_START,
   STATE_FIELDS_START,
   STATE_FIELDS_OBJECT_START,
   STATE_END
};

struct netplay_rooms
{
   struct netplay_room *head;
   struct netplay_room *cur;
};

struct netplay_json_context
{
   bool *cur_member_bool;
   int  *cur_member_int;
   int  *cur_member_inthex;
   char *cur_member_string;
   size_t cur_member_size;
   enum netplay_parse_state state;
};

/* TODO/FIXME - static global variable */
static struct netplay_rooms *netplay_rooms_data;

static bool netplay_json_boolean(void* ctx, bool value)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
      if (pCtx->cur_member_bool)
         *pCtx->cur_member_bool = value;

   return true;
}

static bool netplay_json_string(void* ctx, const char* pValue, size_t length)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      if (pValue && length)
      {
         if (pCtx->cur_member_inthex)
         {
            /* CRC comes in as a string but it is stored
             * as an unsigned casted to int. */
            *pCtx->cur_member_inthex = (int)strtoul(pValue, NULL, 16);
         }
         if (pCtx->cur_member_string)
         {
            strlcpy(pCtx->cur_member_string, pValue, pCtx->cur_member_size);
         }
      }
   }

   return true;
}

static bool netplay_json_number(void* ctx, const char* pValue, size_t length)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      if (pValue && length)
         if (pCtx->cur_member_int)
            *pCtx->cur_member_int = (int)strtol(pValue, NULL, 10);
   }

   return true;
}

static bool netplay_json_start_object(void* ctx)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_FIELDS_START)
   {
      pCtx->state = STATE_FIELDS_OBJECT_START;

      if (!netplay_rooms_data->head)
      {
         netplay_rooms_data->head      = (struct netplay_room*)calloc(1, sizeof(*netplay_rooms_data->head));
         netplay_rooms_data->cur       = netplay_rooms_data->head;
      }
      else if (!netplay_rooms_data->cur->next)
      {
         netplay_rooms_data->cur->next = (struct netplay_room*)calloc(1, sizeof(*netplay_rooms_data->cur->next));
         netplay_rooms_data->cur       = netplay_rooms_data->cur->next;
      }
   }
   else if (pCtx->state == STATE_ARRAY_START)
      pCtx->state = STATE_OBJECT_START;

   return true;
}

static bool netplay_json_end_object(void* ctx)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
      pCtx->state = STATE_ARRAY_START;

   return true;
}

static bool netplay_json_object_member(void* ctx, const char* pValue, size_t length)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (!pValue || !length)
      return true;

   if (pCtx->state == STATE_OBJECT_START && !string_is_empty(pValue)
         && string_is_equal(pValue, "fields"))
      pCtx->state = STATE_FIELDS_START;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      pCtx->cur_member_bool   = NULL;
      pCtx->cur_member_int    = NULL;
      pCtx->cur_member_inthex = NULL;
      pCtx->cur_member_string = NULL;

      if (!string_is_empty(pValue))
      {
         if (string_is_equal(pValue, "username"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->nickname;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->nickname);
         }
         else if (string_is_equal(pValue, "game_name"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->gamename;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->gamename);
         }
         else if (string_is_equal(pValue, "core_name"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->corename;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->corename);
         }
         else if (string_is_equal(pValue, "ip"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->address;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->address);
         }
         else if (string_is_equal(pValue, "port"))
         {
            pCtx->cur_member_int    = &netplay_rooms_data->cur->port;
         }
         else if (string_is_equal(pValue, "game_crc"))
         {
            pCtx->cur_member_inthex = &netplay_rooms_data->cur->gamecrc;
         }
         else if (string_is_equal(pValue, "core_version"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->coreversion;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->coreversion);
         }
         else if (string_is_equal(pValue, "has_password"))
         {
            pCtx->cur_member_bool   = &netplay_rooms_data->cur->has_password;
         }
         else if (string_is_equal(pValue, "has_spectate_password"))
         {
            pCtx->cur_member_bool   = &netplay_rooms_data->cur->has_spectate_password;
         }
         else if (string_is_equal(pValue, "fixed"))
         {
            pCtx->cur_member_bool   = &netplay_rooms_data->cur->fixed;
         }
         else if (string_is_equal(pValue, "mitm_ip"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->mitm_address;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->mitm_address);
         }
         else if (string_is_equal(pValue, "mitm_port"))
         {
            pCtx->cur_member_int    = &netplay_rooms_data->cur->mitm_port;
         }
         else if (string_is_equal(pValue, "host_method"))
         {
            pCtx->cur_member_int    = &netplay_rooms_data->cur->host_method;
         }
         else if (string_is_equal(pValue, "retroarch_version"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->retroarch_version;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->retroarch_version);
         }
         else if (string_is_equal(pValue, "country"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->country;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->country);
         }
         else if (string_is_equal(pValue, "frontend"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->frontend;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->frontend);
         }
         else if (string_is_equal(pValue, "subsystem_name"))
         {
            pCtx->cur_member_string = netplay_rooms_data->cur->subsystem_name;
            pCtx->cur_member_size   = sizeof(netplay_rooms_data->cur->subsystem_name);
         }
      }
   }

   return true;
}

static bool netplay_json_start_array(void* ctx)
{
   struct netplay_json_context* pCtx = (struct netplay_json_context*)ctx;

   if (pCtx->state == STATE_START)
      pCtx->state = STATE_ARRAY_START;

   return true;
}

static void netplay_rooms_error(void *context, int line, int col, const char* error)
{
   RARCH_ERR("[netplay] Error: Invalid JSON at line %d, column %d - %s.\n",
         line, col, error);
}

void netplay_rooms_free(void)
{
   if (netplay_rooms_data)
   {
      struct netplay_room *room = netplay_rooms_data->head;

      if (room)
      {
         while (room)
         {
            struct netplay_room *next = room->next;

            free(room);
            room = next;
         }
      }

      free(netplay_rooms_data);
   }
   netplay_rooms_data = NULL;
}

int netplay_rooms_parse(const char *buf)
{
   struct netplay_json_context ctx;

   memset(&ctx, 0, sizeof(ctx));

   ctx.state = STATE_START;

   /* delete any previous rooms */
   netplay_rooms_free();

   netplay_rooms_data = (struct netplay_rooms*)
      calloc(1, sizeof(*netplay_rooms_data));

   rjson_parse_quick(buf, &ctx, 0,
         netplay_json_object_member,
         netplay_json_string,
         netplay_json_number,
         netplay_json_start_object,
         netplay_json_end_object,
         netplay_json_start_array,
         NULL /* end_array_handler */,
         netplay_json_boolean,
         NULL /* null handler */,
         netplay_rooms_error);

   return 0;
}

struct netplay_room* netplay_room_get(int index)
{
   int                   cur = 0;
   struct netplay_room *room = netplay_rooms_data->head;

   if (index < 0)
      return NULL;

   while (room)
   {
      if (cur == index)
         break;

      room = room->next;
      cur++;
   }

   return room;
}

int netplay_rooms_get_count(void)
{
   int count = 0;
   struct netplay_room *room;

   if (!netplay_rooms_data)
      return count;

   room = netplay_rooms_data->head;

   if (!room)
      return count;

   while (room)
   {
      count++;

      room = room->next;
   }

   return count;
}

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
#include <formats/jsonsax_full.h>
#include "netplay_discovery.h"
#include "../../verbosity.h"

enum parse_state
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

typedef struct tag_Context
{
   JSON_Parser parser;
   enum parse_state state;
   char *cur_field;
   void *cur_member;
} Context;

static struct netplay_rooms *rooms;

static void parse_context_init(Context* pCtx)
{
   pCtx->parser = NULL;
}

static void parse_context_free(Context* pCtx)
{
   if (pCtx->cur_field)
      free(pCtx->cur_field);

   pCtx->cur_field = NULL;

   JSON_Parser_Free(pCtx->parser);
}

static JSON_Parser_HandlerResult JSON_CALL EncodingDetectedHandler(
      JSON_Parser parser)
{
   (void)parser;
   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL NullHandler(
      JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)pCtx;
   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL BooleanHandler(
      JSON_Parser parser, JSON_Boolean value)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
      if (pCtx->cur_field)
         *((bool*)pCtx->cur_member) = value;

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StringHandler(
      JSON_Parser parser, char* pValue, size_t length,
      JSON_StringAttributes attributes)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)attributes;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      if (pValue && length)
      {
         if (pCtx->cur_field)
         {
            /* CRC comes in as a string but it is stored
             * as an unsigned casted to int. */
            if (string_is_equal(pCtx->cur_field, "game_crc"))
               *((int*)pCtx->cur_member) = (int)strtoul(pValue, NULL, 16);
            else
               strlcpy((char*)pCtx->cur_member, pValue, PATH_MAX_LENGTH);
         }
      }
   }

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL NumberHandler(
      JSON_Parser parser, char* pValue, size_t length, JSON_NumberAttributes attributes)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)attributes;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      if (pValue && length)
         if (pCtx->cur_field)
            *((int*)pCtx->cur_member) = (int)strtol(pValue, NULL, 10);
   }

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL SpecialNumberHandler(
      JSON_Parser parser, JSON_SpecialNumber value)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)pCtx;
   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StartObjectHandler(JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;

   if (pCtx->state == STATE_FIELDS_START)
   {
      pCtx->state = STATE_FIELDS_OBJECT_START;

      if (!rooms->head)
      {
         rooms->head      = (struct netplay_room*)calloc(1, sizeof(*rooms->head));
         rooms->cur       = rooms->head;
      }
      else if (!rooms->cur->next)
      {
         rooms->cur->next = (struct netplay_room*)calloc(1, sizeof(*rooms->cur->next));
         rooms->cur       = rooms->cur->next;
      }
   }
   else if (pCtx->state == STATE_ARRAY_START)
      pCtx->state = STATE_OBJECT_START;

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL EndObjectHandler(JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
      pCtx->state = STATE_ARRAY_START;

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL ObjectMemberHandler(JSON_Parser parser,
      char* pValue, size_t length, JSON_StringAttributes attributes)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)attributes;

   if (!pValue || !length)
      return JSON_Parser_Continue;

   if (pCtx->state == STATE_OBJECT_START && !string_is_empty(pValue)
         && string_is_equal(pValue, "fields"))
      pCtx->state = STATE_FIELDS_START;

   if (pCtx->state == STATE_FIELDS_OBJECT_START)
   {
      if (pCtx->cur_field)
         free(pCtx->cur_field);
      pCtx->cur_field = NULL;

      if (!string_is_empty(pValue))
      {
         if (string_is_equal(pValue, "username"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->nickname;
         }
         else if (string_is_equal(pValue, "game_name"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->gamename;
         }
         else if (string_is_equal(pValue, "core_name"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->corename;
         }
         else if (string_is_equal(pValue, "ip"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->address;
         }
         else if (string_is_equal(pValue, "port"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->port;
         }
         else if (string_is_equal(pValue, "game_crc"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->gamecrc;
         }
         else if (string_is_equal(pValue, "core_version"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->coreversion;
         }
         else if (string_is_equal(pValue, "has_password"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->has_password;
         }
         else if (string_is_equal(pValue, "has_spectate_password"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->has_spectate_password;
         }
         else if (string_is_equal(pValue, "fixed"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->fixed;
         }
         else if (string_is_equal(pValue, "mitm_ip"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->mitm_address;
         }
         else if (string_is_equal(pValue, "mitm_port"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->mitm_port;
         }
         else if (string_is_equal(pValue, "host_method"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->host_method;
         }
         else if (string_is_equal(pValue, "retroarch_version"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->retroarch_version;
         }
         else if (string_is_equal(pValue, "country"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->country;
         }
         else if (string_is_equal(pValue, "frontend"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->frontend;
         }
         else if (string_is_equal(pValue, "subsystem_name"))
         {
            pCtx->cur_field       = strdup(pValue);
            pCtx->cur_member      = &rooms->cur->subsystem_name;
         }
      }
   }

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StartArrayHandler(JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;

   if (pCtx->state == STATE_START)
      pCtx->state = STATE_ARRAY_START;

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL EndArrayHandler(JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)pCtx;
   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL ArrayItemHandler(JSON_Parser parser)
{
   Context* pCtx = (Context*)JSON_Parser_GetUserData(parser);
   (void)parser;
   (void)pCtx;
   return JSON_Parser_Continue;
}

static int parse_context_setup(Context* pCtx)
{
   if (JSON_Parser_GetInputEncoding(pCtx->parser) == JSON_UnknownEncoding)
   {
      JSON_Parser_SetEncodingDetectedHandler(pCtx->parser, &EncodingDetectedHandler);
   }

   JSON_Parser_SetNullHandler(pCtx->parser, &NullHandler);
   JSON_Parser_SetBooleanHandler(pCtx->parser, &BooleanHandler);
   JSON_Parser_SetStringHandler(pCtx->parser, &StringHandler);
   JSON_Parser_SetNumberHandler(pCtx->parser, &NumberHandler);
   JSON_Parser_SetSpecialNumberHandler(pCtx->parser, &SpecialNumberHandler);
   JSON_Parser_SetStartObjectHandler(pCtx->parser, &StartObjectHandler);
   JSON_Parser_SetEndObjectHandler(pCtx->parser, &EndObjectHandler);
   JSON_Parser_SetObjectMemberHandler(pCtx->parser, &ObjectMemberHandler);
   JSON_Parser_SetStartArrayHandler(pCtx->parser, &StartArrayHandler);
   JSON_Parser_SetEndArrayHandler(pCtx->parser, &EndArrayHandler);
   JSON_Parser_SetArrayItemHandler(pCtx->parser, &ArrayItemHandler);
   JSON_Parser_SetUserData(pCtx->parser, pCtx);

   return 1;
}

static void parse_context_error(Context* pCtx)
{
   if (JSON_Parser_GetError(pCtx->parser) != JSON_Error_AbortedByHandler)
   {
      JSON_Error error            = JSON_Parser_GetError(pCtx->parser);
      JSON_Location errorLocation = {0, 0, 0};

      (void)JSON_Parser_GetErrorLocation(pCtx->parser, &errorLocation);

      RARCH_ERR("invalid JSON at line %d, column %d (input byte %d) - %s.\n",
            (int)errorLocation.line + 1,
            (int)errorLocation.column + 1,
            (int)errorLocation.byte,
            JSON_ErrorString(error));
   }
}

static int json_parse(Context* pCtx, const char *buf)
{
   if (!JSON_Parser_Parse(pCtx->parser, buf, strlen(buf), JSON_True))
   {
      parse_context_error(pCtx);
      return 0;
   }

   return 1;
}

void netplay_rooms_free(void)
{
   if (rooms)
   {
      struct netplay_room *room = rooms->head;

      if (room)
      {
         while (room != NULL)
         {
            struct netplay_room *next = room->next;

            free(room);
            room = next;
         }
      }

      free(rooms);
      rooms = NULL;
   }
}

int netplay_rooms_parse(const char *buf)
{
   Context ctx;

   memset(&ctx, 0, sizeof(ctx));

   ctx.state = STATE_START;

   /* delete any previous rooms */
   netplay_rooms_free();

   rooms = (struct netplay_rooms*)calloc(1, sizeof(*rooms));

   parse_context_init(&ctx);

   ctx.parser = JSON_Parser_Create(NULL);

   if (!ctx.parser)
   {
      RARCH_ERR("could not allocate memory for JSON parser.\n");
      return 1;
   }

   parse_context_setup(&ctx);
   json_parse(&ctx, buf);
   parse_context_free(&ctx);

   return 0;
}

struct netplay_room* netplay_room_get(int index)
{
   int cur = 0;
   struct netplay_room *room = rooms->head;

   if (index < 0)
      return NULL;

   while (room != NULL)
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

   if (!rooms)
      return count;

   room = rooms->head;

   if (!room)
      return count;

   while(room != NULL)
   {
      count++;

      room = room->next;
   }

   return count;
}

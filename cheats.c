/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cheats.h"
#include "sha256.h"
#include "dynamic.h"
#include "general.h"
#include "strl.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

struct cheat
{
   char *desc;
   bool state;
   char *code;
};

struct cheat_manager
{
   struct cheat *cheats;
   unsigned ptr;
   unsigned size;
   unsigned buf_size;
};

static char* strcat_alloc(char *dest, const char *input)
{
   size_t dest_len = dest ? strlen(dest) : 0;
   size_t input_len = strlen(input);
   size_t required_len = dest_len + input_len + 1;

   char *output = realloc(dest, required_len);
   assert(output);

   if (dest)
      strlcat(output, input, required_len);
   else
      strlcpy(output, input, required_len);

   return output;
}

static void xml_grab_cheat(struct cheat *cht, xmlNodePtr ptr)
{
   memset(cht, 0, sizeof(struct cheat));
   bool first = true;
   for (xmlNodePtr node = ptr; node; node = node->next)
   {
      if (strcmp((const char*)node->name, "description") == 0)
      {
         cht->desc = (char*)xmlNodeGetContent(node);
      }
      else if (strcmp((const char*)node->name, "code") == 0)
      {
         if (!first)
            cht->code = strcat_alloc(cht->code, "+");

         xmlChar *code = xmlNodeGetContent(node);
         assert(code);

         cht->code = strcat_alloc(cht->code, (const char*)code);
         xmlFree(code);

         first = false;
      }
   }
}

static void xml_grab_cheats(cheat_manager_t *handle, xmlNodePtr ptr)
{
   xmlNodePtr node = NULL;
   for (node = ptr; node; node = node->next)
   {
      if (strcmp((const char*)node->name, "name") == 0)
      {
         xmlChar *name = xmlNodeGetContent(node);
         if (name)
         {
            SSNES_LOG("Found cheat for game: \"%s\"\n", name);
            xmlFree(name);
         }
      }
      else if (strcmp((const char*)node->name, "cheat") == 0)
      {
         if (handle->size == handle->buf_size)
         {
            handle->buf_size *= 2;
            handle->cheats = realloc(handle->cheats, handle->buf_size * sizeof(struct cheat));
            assert(handle->cheats);
         }

         xml_grab_cheat(&handle->cheats[handle->size++], node->children);
      }
   }
}

cheat_manager_t* cheat_manager_new(const char *path)
{
   psnes_cheat_reset();

   xmlParserCtxtPtr ctx = NULL;
   xmlDocPtr doc = NULL;
   cheat_manager_t *handle = calloc(1, sizeof(handle));
   if (!handle)
      return NULL;

   handle->buf_size = 32;
   handle->cheats = malloc(handle->buf_size * sizeof(struct cheat));
   if (!handle->cheats)
   {
      handle->buf_size = 0;
      goto error;
   }

   ctx = xmlNewParserCtxt();
   if (!ctx)
      goto error;

   doc = xmlCtxtReadFile(ctx, path, NULL, 0);
   if (!doc)
   {
      SSNES_ERR("Failed to parse XML file: %s\n", path);
      goto error;
   }

   if (ctx->valid == 0)
   {
      SSNES_ERR("Cannot validate XML file: %s\n", path);
      goto error;
   }

   xmlNodePtr head = xmlDocGetRootElement(doc);
   xmlNodePtr cur = NULL;
   for (cur = head; cur; cur = cur->next)
   {
      if (cur->type == XML_ELEMENT_NODE && strcmp((const char*)cur->name, "database") == 0)
         break;
   }

   if (!cur)
      goto error;

   for (cur = cur->children; cur; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;

      if (strcmp((const char*)cur->name, "cartridge") == 0)
      {
         xmlChar *sha256 = xmlGetProp(cur, (const xmlChar*)"sha256");
         if (!sha256)
            continue;

         if (memcmp(sha256, g_extern.sha256, 64) == 0)
         {
            xmlFree(sha256);
            break;
         }

         xmlFree(sha256);
      }
   }

   if (!cur)
      goto error;

   xml_grab_cheats(handle, cur->children);

   xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return handle;

error:
   cheat_manager_free(handle);
   if (doc)
      xmlFreeDoc(doc);
   if (ctx)
      xmlFreeParserCtxt(ctx);
   return NULL;
}

void cheat_manager_free(cheat_manager_t *handle)
{
   if (!handle)
      return;

   if (handle->cheats)
   {
      for (unsigned i = 0; i < handle->size; i++)
      {
         xmlFree(handle->cheats[i].desc);
         free(handle->cheats[i].code);
      }

      free(handle->cheats);
   }

   free(handle);
}

void cheat_manager_toggle(cheat_manager_t *handle)
{
   handle->cheats[handle->ptr].state ^= true;
   psnes_cheat_set(handle->ptr, handle->cheats[handle->ptr].state, handle->cheats[handle->ptr].code);
}


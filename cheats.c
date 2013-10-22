/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "cheats.h"
#include "hash.h"
#include "dynamic.h"
#include "general.h"
#include "compat/strl.h"
#include "compat/posix_string.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "conf/config_file.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/tree.h>
#else
#define RXML_LIBXML2_COMPAT
#include "compat/rxml/rxml.h"
#endif

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

static char *strcat_alloc(char *dest, const char *input)
{
   size_t dest_len = dest ? strlen(dest) : 0;
   size_t input_len = strlen(input);
   size_t required_len = dest_len + input_len + 1;

   char *output = (char*)realloc(dest, required_len);
   if (!output)
      return NULL;

   if (dest)
      strlcat(output, input, required_len);
   else
      strlcpy(output, input, required_len);

   return output;
}

static bool xml_grab_cheat(struct cheat *cht, xmlNodePtr ptr)
{
   if (!ptr)
      return false;

   memset(cht, 0, sizeof(struct cheat));
   bool first = true;

   for (; ptr; ptr = ptr->next)
   {
      if (strcmp((const char*)ptr->name, "description") == 0)
      {
         cht->desc = (char*)xmlNodeGetContent(ptr);
      }
      else if (strcmp((const char*)ptr->name, "code") == 0)
      {
         if (!first)
         {
            cht->code = strcat_alloc(cht->code, "+");
            if (!cht->code)
               return false;
         }

         xmlChar *code = xmlNodeGetContent(ptr);
         if (!code)
            return false;

         cht->code = strcat_alloc(cht->code, (const char*)code);
         xmlFree(code);
         if (!cht->code)
            return false;

         first = false;
      }
   }

   return true;
}

static bool xml_grab_cheats(cheat_manager_t *handle, xmlNodePtr ptr)
{
   for (; ptr; ptr = ptr->next)
   {
      if (strcmp((const char*)ptr->name, "name") == 0)
      {
         xmlChar *name = xmlNodeGetContent(ptr);
         if (name)
         {
            RARCH_LOG("Found cheat for game: \"%s\"\n", name);
            xmlFree(name);
         }
      }
      else if (strcmp((const char*)ptr->name, "cheat") == 0)
      {
         if (handle->size == handle->buf_size)
         {
            handle->buf_size *= 2;
            handle->cheats = (struct cheat*)realloc(handle->cheats, handle->buf_size * sizeof(struct cheat));
            if (!handle->cheats)
               return false;
         }

         if (xml_grab_cheat(&handle->cheats[handle->size], ptr->children))
            handle->size++;
      }
   }

   return true;
}

static void cheat_manager_apply_cheats(cheat_manager_t *handle)
{
   unsigned i, index;
   index = 0;
   pretro_cheat_reset();
   for (i = 0; i < handle->size; i++)
   {
      if (handle->cheats[i].state)
         pretro_cheat_set(index++, true, handle->cheats[i].code);
   }
}

static void cheat_manager_load_config(cheat_manager_t *handle, const char *path, const char *sha256)
{
   if (!(*path))
      return;

   config_file_t *conf = config_file_new(path);
   if (!conf)
      return;

   char *str = NULL;
   if (!config_get_string(conf, sha256, &str))
   {
      config_file_free(conf);
      return;
   }

   char *save;
   const char *num = strtok_r(str, ";", &save);
   while (num)
   {
      unsigned index = strtoul(num, NULL, 0);
      if (index < handle->size)
         handle->cheats[index].state = true;

      num = strtok_r(NULL, ";", &save);
   }

   free(str);
   config_file_free(conf);

   cheat_manager_apply_cheats(handle);
}

static void cheat_manager_save_config(cheat_manager_t *handle, const char *path, const char *sha256)
{
   if (!(*path))
      return;

   config_file_t *conf = config_file_new(path);
   if (!conf)
      conf = config_file_new(NULL);

   if (!conf)
   {
      RARCH_ERR("Cannot save XML cheat settings.\n");
      return;
   }

   unsigned i;
   char conf_str[512] = {0};
   char tmp[32] = {0};

   for (i = 0; i < handle->size; i++)
   {
      if (handle->cheats[i].state)
      {
         snprintf(tmp, sizeof(tmp), "%u;", i);
         strlcat(conf_str, tmp, sizeof(conf_str));
      }
   }

   if (*conf_str)
      conf_str[strlen(conf_str) - 1] = '\0'; // Remove the trailing ';'

   config_set_string(conf, sha256, conf_str);

   if (!config_file_write(conf, path))
      RARCH_ERR("Failed to write XML cheat settings to \"%s\". Check permissions.\n", path);

   config_file_free(conf);
}

cheat_manager_t *cheat_manager_new(const char *path)
{
   LIBXML_TEST_VERSION;

   pretro_cheat_reset();

   xmlParserCtxtPtr ctx = NULL;
   xmlDocPtr doc = NULL;
   cheat_manager_t *handle = (cheat_manager_t*)calloc(1, sizeof(struct cheat_manager));
   if (!handle)
      return NULL;

   xmlNodePtr head = NULL;
   xmlNodePtr cur = NULL;

   handle->buf_size = 1;
   handle->cheats = (struct cheat*)calloc(handle->buf_size, sizeof(struct cheat));
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
      RARCH_ERR("Failed to parse XML file: %s\n", path);
      goto error;
   }

#ifdef HAVE_LIBXML2
   if (ctx->valid == 0)
   {
      RARCH_ERR("Cannot validate XML file: %s\n", path);
      goto error;
   }
#endif

   head = xmlDocGetRootElement(doc);
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

         if (*g_extern.sha256 && strcmp((const char*)sha256, g_extern.sha256) == 0)
         {
            xmlFree(sha256);
            break;
         }

         xmlFree(sha256);
      }
   }

   if (!cur)
      goto error;

   if (!xml_grab_cheats(handle, cur->children))
   {
      RARCH_ERR("Failed to grab cheats. This should not happen.\n");
      goto error;
   }

   if (handle->size == 0)
   {
      RARCH_ERR("Did not find any cheats in XML file: %s\n", path);
      goto error;
   }

   cheat_manager_load_config(handle, g_settings.cheat_settings_path, g_extern.sha256);

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
   unsigned i;
   if (!handle)
      return;

   if (handle->cheats)
   {
      cheat_manager_save_config(handle, g_settings.cheat_settings_path, g_extern.sha256);
      for (i = 0; i < handle->size; i++)
      {
         xmlFree(handle->cheats[i].desc);
         free(handle->cheats[i].code);
      }

      free(handle->cheats);
   }

   free(handle);
}

static void cheat_manager_update(cheat_manager_t *handle)
{
   msg_queue_clear(g_extern.msg_queue);
   char msg[256];
   snprintf(msg, sizeof(msg), "Cheat: #%u [%s]: %s", handle->ptr, handle->cheats[handle->ptr].state ? "ON" : "OFF", handle->cheats[handle->ptr].desc);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);
}


void cheat_manager_toggle(cheat_manager_t *handle)
{
   handle->cheats[handle->ptr].state ^= true;
   cheat_manager_apply_cheats(handle);
   cheat_manager_update(handle);
}

void cheat_manager_index_next(cheat_manager_t *handle)
{
   handle->ptr = (handle->ptr + 1) % handle->size;
   cheat_manager_update(handle);
}

void cheat_manager_index_prev(cheat_manager_t *handle)
{
   if (handle->ptr == 0)
      handle->ptr = handle->size - 1;
   else
      handle->ptr--;

   cheat_manager_update(handle);
}


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

#include "rxml.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include "../../boolean.h"
#include "../posix_string.h"

#ifndef RXML_TEST
#include "../../general.h"
#endif

struct rxml_document
{
   struct rxml_node *root_node;
};

struct rxml_node *rxml_root_node(rxml_document_t *doc)
{
   return doc->root_node;
}

static void rxml_free_node(struct rxml_node *node)
{
   struct rxml_node *head;
   for (head = node->children; head; )
   {
      struct rxml_node *next_node = head->next;
      rxml_free_node(head);
      head = next_node;
   }

   struct rxml_attrib_node *attrib_node_head;
   for (attrib_node_head = node->attrib; attrib_node_head; )
   {
      struct rxml_attrib_node *next_attrib = attrib_node_head->next;

      free(attrib_node_head->attrib);
      free(attrib_node_head->value);
      free(attrib_node_head);

      attrib_node_head = next_attrib;
   }

   free(node->name);
   free(node->data);
   free(node);
}

static bool validate_header(const char **ptr)
{
   if (memcmp(*ptr, "<?xml", 5) == 0)
   {
      const char *eol = strstr(*ptr, "?>\n");
      if (!eol)
         return false;

      // Always use UTF-8. Don't really care to check.
      *ptr = eol + 3;
      return true;
   }
   else
      return true;
}

static bool range_is_space(const char *begin, const char *end)
{
   for (; begin < end; begin++)
      if (!isspace(*begin))
         return false;

   return true;
}

static void skip_spaces(const char **ptr_)
{
   const char *ptr = *ptr_;
   while (isspace(*ptr))
      ptr++;

   *ptr_ = ptr;
}

static char *strdup_range(const char *begin, const char *end)
{
   ptrdiff_t len = end - begin;

   char *ret = (char*)malloc(len + 1);
   if (!ret)
      return NULL;

   memcpy(ret, begin, len);
   ret[len] = '\0';
   return ret;
}

static char *strdup_range_escape(const char *begin, const char *end)
{
   return strdup_range(begin, end); // Escaping is ignored. Assume we don't deal with that.
}

static struct rxml_attrib_node *rxml_parse_attrs(const char *str)
{
   char *copy = strdup(str);
   if (!copy)
      return NULL;

   char *last_char = copy + strlen(copy) - 1;
   if (*last_char == '/')
      *last_char = '\0';

   struct rxml_attrib_node *list = NULL;
   struct rxml_attrib_node *tail = NULL;

   char *save;
   const char *elem = strtok_r(copy, " \n\t\f\v\r", &save);
   while (elem)
   {
      const char *eq = strstr(elem, "=\"");
      if (!eq)
         goto end;

      const char *end = strrchr(eq + 2, '\"');
      if (!end || end != (elem + strlen(elem) - 1))
         goto end;

      char *attrib = strdup_range_escape(elem, eq);
      char *value  = strdup_range_escape(eq + 2, end);
      if (!attrib || !value)
         goto end;

      struct rxml_attrib_node *new_node = (struct rxml_attrib_node*)calloc(1, sizeof(*new_node));
      if (!new_node)
         goto end;

      new_node->attrib = attrib;
      new_node->value  = value;

      if (tail)
      {
         tail->next = new_node;
         tail = new_node;
      }
      else
         list = tail = new_node;

      elem = strtok_r(NULL, " \n\t\f\v\r", &save);
   }

end:
   free(copy);
   return list;
}

static char *find_first_space(const char *str)
{
   while (*str && !isspace(*str))
      str++;

   return isspace(*str) ? (char*)str : NULL;
}

static bool rxml_parse_tag(struct rxml_node *node, const char *str)
{
   const char *str_ptr = str;
   skip_spaces(&str_ptr);

   const char *name_end = find_first_space(str_ptr);
   if (name_end)
   {
      node->name = strdup_range(str_ptr, name_end);
      if (!node->name || !*node->name)
         return false;

      node->attrib = rxml_parse_attrs(name_end);
      return true;
   }
   else
   {
      node->name = strdup(str_ptr);
      return node->name && *node->name;
   }
}

static struct rxml_node *rxml_parse_node(const char **ptr_)
{
   const char *ptr     = NULL;
   const char *closing = NULL;
   char *str           = NULL;
   bool is_closing     = false;

   struct rxml_node *node = (struct rxml_node*)calloc(1, sizeof(*node));
   if (!node)
      return NULL;

   skip_spaces(ptr_);

   ptr = *ptr_;
   if (*ptr != '<')
      goto error;

   closing = strchr(ptr, '>');
   if (!closing)
      goto error;

   str = strdup_range(ptr + 1, closing);
   if (!str)
      goto error;

   if (!rxml_parse_tag(node, str))
      goto error;

   is_closing = strstr(ptr, "/>") + 1 == closing; // Are spaces between / and > allowed?

   // Look for more data. Either child nodes or data.
   if (!is_closing)
   {
      size_t closing_tag_size = strlen(node->name) + 4;
      char *closing_tag = (char*)malloc(closing_tag_size);

      const char *cdata_start = NULL;
      const char *child_start = NULL;
      const char *closing_start = NULL;

      if (!closing_tag)
      {
         free(closing_tag);
         goto error;
      }

      snprintf(closing_tag, closing_tag_size, "</%s>", node->name);

      cdata_start   = strstr(closing + 1, "<![CDATA[");
      child_start   = strchr(closing + 1, '<');
      closing_start = strstr(closing + 1, closing_tag);

      if (!closing_start)
      {
         free(closing_tag);
         goto error;
      }

      if (cdata_start && range_is_space(closing + 1, cdata_start)) // CDATA section
      {
         const char *cdata_end = strstr(cdata_start, "]]>");
         if (!cdata_end)
         {
            free(closing_tag);
            goto error;
         }

         node->data = strdup_range(cdata_start + strlen("<![CDATA["), cdata_end);
      }
      else if (closing_start && closing_start == child_start) // Simple Data
         node->data = strdup_range(closing + 1, closing_start);
      else // Parse all child nodes.
      {
         struct rxml_node *list = NULL;
         struct rxml_node *tail = NULL;

         const char *ptr = child_start;

         const char *first_start   = strchr(ptr, '<');
         const char *first_closing = strstr(ptr, "</");
         while (first_start && first_closing && first_start < first_closing)
         {
            struct rxml_node *new_node = rxml_parse_node(&ptr);
            if (!new_node)
            {
               free(closing_tag);
               goto error;
            }

            if (tail)
            {
               tail->next = new_node;
               tail = new_node;
            }
            else
               list = tail = new_node;

            first_start   = strchr(ptr, '<');
            first_closing = strstr(ptr, "</");
         }

         node->children = list;

         closing_start = strstr(ptr, closing_tag);
         if (!closing_start)
         {
            free(closing_tag);
            goto error;
         }
      }

      *ptr_ = closing_start + strlen(closing_tag);
      free(closing_tag);
   }
   else
      *ptr_ = closing + 1;

   free(str);
   return node;

error:
   free(str);
   rxml_free_node(node);
   return NULL;
}

static char *purge_xml_comments(const char *str)
{
   size_t len = strlen(str);
   char *new_str = (char*)malloc(len + 1);
   if (!new_str)
      return NULL;

   new_str[len] = '\0';

   char *copy_dest       = new_str;
   const char *copy_src  = str;
   for (;;)
   {
      const char *comment_start = strstr(copy_src, "<!--");
      const char *comment_end   = strstr(copy_src, "-->");

      if (!comment_start || !comment_end)
         break;

      ptrdiff_t copy_len = comment_start - copy_src;
      memcpy(copy_dest, copy_src, copy_len);

      copy_dest += copy_len;
      copy_src   = comment_end + strlen("-->");
   }

   // Avoid strcpy() as OpenBSD is anal and hates you for using it even when it's perfectly safe.
   len = strlen(copy_src);
   memcpy(copy_dest, copy_src, len);
   copy_dest[len] = '\0';

   return new_str;
}

rxml_document_t *rxml_load_document(const char *path)
{
#ifndef RXML_TEST
   RARCH_WARN("Using RXML as drop in for libxml2. Behavior might be very buggy.\n");
#endif

   char *memory_buffer     = NULL;
   char *new_memory_buffer = NULL;
   const char *mem_ptr     = NULL;
   long len                = 0;

   FILE *file = fopen(path, "r");
   if (!file)
      return NULL;

   rxml_document_t *doc = (rxml_document_t*)calloc(1, sizeof(*doc));
   if (!doc)
      goto error;

   fseek(file, 0, SEEK_END);
   len = ftell(file);
   rewind(file);

   memory_buffer = (char*)malloc(len + 1);
   if (!memory_buffer)
      goto error;

   memory_buffer[len] = '\0';
   if (fread(memory_buffer, 1, len, file) != (size_t)len)
      goto error;

   fclose(file);
   file = NULL;

   mem_ptr = memory_buffer;

   if (!validate_header(&mem_ptr))
      goto error;

   new_memory_buffer = purge_xml_comments(mem_ptr);
   if (!new_memory_buffer)
      goto error;

   free(memory_buffer);
   mem_ptr = memory_buffer = new_memory_buffer;

   doc->root_node = rxml_parse_node(&mem_ptr);
   if (!doc->root_node)
      goto error;

   free(memory_buffer);
   return doc;

error:
   free(memory_buffer);
   if (file)
      fclose(file);
   rxml_free_document(doc);
   return NULL;
}

void rxml_free_document(rxml_document_t *doc)
{
   if (!doc)
      return;

   if (doc->root_node)
      rxml_free_node(doc->root_node);

   free(doc);
}

char *rxml_node_attrib(struct rxml_node *node, const char *attrib)
{
   struct rxml_attrib_node *attribs;
   for (attribs = node->attrib; attribs; attribs = attribs->next)
   {
      if (!strcmp(attrib, attribs->attrib))
         return attribs->value;
   }

   return NULL;
}


/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <compat/posix_string.h>

#include <formats/rxml.h>

struct rxml_document
{
   struct rxml_node *root_node;
};

struct rxml_node *rxml_root_node(rxml_document_t *doc)
{
   if (doc)
      return doc->root_node;
   return NULL;
}

static void rxml_free_node(struct rxml_node *node)
{
   struct rxml_node *head = NULL;
   struct rxml_attrib_node *attrib_node_head = NULL;

   if (!node)
      return;

   for (head = node->children; head; )
   {
      struct rxml_node *next_node = (struct rxml_node*)head->next;
      rxml_free_node(head);
      head = next_node;
   }

   for (attrib_node_head = node->attrib; attrib_node_head; )
   {
      struct rxml_attrib_node *next_attrib = NULL;

      if (!attrib_node_head)
         continue;
      
      next_attrib = (struct rxml_attrib_node*)attrib_node_head->next;

      if (!next_attrib)
         continue;

      if (attrib_node_head->attrib)
         free(attrib_node_head->attrib);
      if (attrib_node_head->value)
         free(attrib_node_head->value);
      if (attrib_node_head)
         free(attrib_node_head);

      attrib_node_head = next_attrib;
   }

   if (node->name)
      free(node->name);
   if (node->data)
      free(node->data);
   if (node)
      free(node);
}

static bool validate_header(const char **ptr)
{
   if (memcmp(*ptr, "<?xml", 5) == 0)
   {
      const char *eol = strstr(*ptr, "?>\n");
      if (!eol)
         return false;

      /* Always use UTF-8. Don't really care to check. */
      *ptr = eol + 3;
      return true;
   }
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
   /* Escaping is ignored. Assume we don't deal with that. */
   return strdup_range(begin, end);
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

   char *attrib = NULL;
   char *value = NULL;
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

      attrib = strdup_range_escape(elem, eq);
      value  = strdup_range_escape(eq + 2, end);
      if (!attrib || !value)
         goto end;

      struct rxml_attrib_node *new_node = 
         (struct rxml_attrib_node*)calloc(1, sizeof(*new_node));
      if (!new_node)
         goto end;

      new_node->attrib = attrib;
      new_node->value  = value;
      attrib = NULL;
      value = NULL;

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
   if (copy)
      free(copy);
   if (attrib)
      free(attrib);
   if (value)
      free(value);
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

   /* Are spaces between / and > allowed? */
   is_closing = strstr(ptr, "/>") + 1 == closing;

   /* Look for more data. Either child nodes or data. */
   if (!is_closing)
   {
      size_t closing_tag_size = strlen(node->name) + 4;
      char *closing_tag = (char*)malloc(closing_tag_size);

      const char *cdata_start = NULL;
      const char *child_start = NULL;
      const char *closing_start = NULL;

      if (!closing_tag)
         goto error;

      snprintf(closing_tag, closing_tag_size, "</%s>", node->name);

      cdata_start   = strstr(closing + 1, "<![CDATA[");
      child_start   = strchr(closing + 1, '<');
      closing_start = strstr(closing + 1, closing_tag);

      if (!closing_start)
      {
         free(closing_tag);
         goto error;
      }

      if (cdata_start && range_is_space(closing + 1, cdata_start))
      {
         /* CDATA section */
         const char *cdata_end = strstr(cdata_start, "]]>");
         if (!cdata_end)
         {
            free(closing_tag);
            goto error;
         }

         node->data = strdup_range(cdata_start + 
               strlen("<![CDATA["), cdata_end);
      }
      else if (closing_start && closing_start == child_start) /* Simple Data */
         node->data = strdup_range(closing + 1, closing_start);
      else
      {
         /* Parse all child nodes. */
         struct rxml_node *list = NULL;
         struct rxml_node *tail = NULL;
         const char *first_start = NULL;
         const char *first_closing = NULL;

         ptr           = child_start;
         first_start   = strchr(ptr, '<');
         first_closing = strstr(ptr, "</");
          
         while (
                first_start &&
                first_closing &&
                (first_start < first_closing)
                )
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

   if (str)
      free(str);
   return node;

error:
   if (str)
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
      ptrdiff_t copy_len;
      const char *comment_start = strstr(copy_src, "<!--");
      const char *comment_end   = strstr(copy_src, "-->");

      if (!comment_start || !comment_end)
         break;

      copy_len = comment_start - copy_src;
      memcpy(copy_dest, copy_src, copy_len);

      copy_dest += copy_len;
      copy_src   = comment_end + strlen("-->");
   }

   /* Avoid strcpy() as OpenBSD is anal and hates you 
    * for using it even when it's perfectly safe. */
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
   RFILE *file             = filestream_fopen(path, RFILE_MODE_READ, -1);
   if (!file)
      return NULL;

   rxml_document_t *doc = (rxml_document_t*)calloc(1, sizeof(*doc));
   if (!doc)
      goto error;

   filestream_fseek(file, 0, SEEK_END);
   len = filestream_ftell(file);
   filestream_frewind(file);

   memory_buffer = (char*)malloc(len + 1);
   if (!memory_buffer)
      goto error;

   memory_buffer[len] = '\0';
   if (filestream_fread(file, memory_buffer, len) != (size_t)len)
      goto error;

   filestream_fclose(file);
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
   filestream_fclose(file);
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
   struct rxml_attrib_node *attribs = NULL;
   for (attribs = node->attrib; attribs; attribs = attribs->next)
   {
      if (!strcmp(attrib, attribs->attrib))
         return attribs->value;
   }

   return NULL;
}


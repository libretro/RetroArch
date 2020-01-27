/* Copyright  (C) 2010-2018 The RetroArch team
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

#include <boolean.h>
#include <streams/file_stream.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>

#include <formats/rxml.h>

#include "../../deps/yxml/yxml.h"

#define BUFSIZE 4096

struct rxml_parse_buffer {
   char xml[BUFSIZE];
   char val[BUFSIZE];
   rxml_node_t* stack[32];
};

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
      struct rxml_attrib_node *next_attrib =
            (struct rxml_attrib_node*)attrib_node_head->next;

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

rxml_document_t *rxml_load_document(const char *path)
{
   rxml_document_t *doc;
   char *memory_buffer     = NULL;
   long len                = 0;
   RFILE *file             = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return NULL;

   len           = filestream_get_size(file);
   memory_buffer = (char*)malloc(len + 1);
   if (!memory_buffer)
      goto error;

   memory_buffer[len] = '\0';
   if (filestream_read(file, memory_buffer, len) != (size_t)len)
      goto error;

   filestream_close(file);
   file = NULL;

   doc = rxml_load_document_string(memory_buffer);

   free(memory_buffer);
   return doc;

error:
   free(memory_buffer);
   if(file)
      filestream_close(file);
   return NULL;
}

rxml_document_t *rxml_load_document_string(const char *str)
{
   rxml_document_t *doc          = NULL;
   struct rxml_parse_buffer *buf = NULL;
   size_t stack_i                = 0;
   size_t level                  = 0;
   int c                         = 0;
   char *valptr                  = NULL;
   yxml_t x;

   rxml_node_t *node             = NULL;
   struct rxml_attrib_node *attr = NULL;

   buf = (struct rxml_parse_buffer*)malloc(sizeof(*buf));
   if (!buf)
      goto error;

   valptr = buf->val;

   doc = (rxml_document_t*)calloc(1, sizeof(*doc));
   if (!doc)
      goto error;

   yxml_init(&x, buf->xml, BUFSIZE);

   for (; *str; ++str) {
      yxml_ret_t r = yxml_parse(&x, *str);

      if (r < 0)
         goto error;

      switch (r) {

      case YXML_ELEMSTART:
         if (node) {

            if (level > stack_i) {
               buf->stack[stack_i] = node;
               ++stack_i;

               node->children = (rxml_node_t*)calloc(1, sizeof(*node));
               node = node->children;
            }
            else {
               node->next = (rxml_node_t*)calloc(1, sizeof(*node));
               node = node->next;
            }
         }
         else {
            node = doc->root_node = (rxml_node_t*)calloc(1, sizeof(*node));
         }

         if (node->name)
            free(node->name);
         node->name = strdup(x.elem);

         attr = NULL;

         ++level;
         break;

      case YXML_ELEMEND:
         --level;

         if (valptr > buf->val) {
            *valptr = '\0';

            /* Original code was broken here:
             * > If an element ended on two successive
             *   iterations, on the second iteration
             *   the 'data' for the *previous* node would
             *   get overwritten
             * > This effectively erased the data for the
             *   previous node, *and* caused a memory leak
             *   (due to the double strdup())
             * It seems the correct thing to do here is
             * only copy the data if the current 'level'
             * and 'stack index' are the same... */
            if (level == stack_i)
            {
               if (node->data)
                  free(node->data);
               node->data = strdup(buf->val);
            }

            valptr = buf->val;
         }

         if (level < stack_i) {
            --stack_i;
            node = buf->stack[stack_i];
         }
         break;

      case YXML_CONTENT:
         for (c = 0; c < sizeof(x.data) && x.data[c]; ++c) {
            *valptr = x.data[c];
            ++valptr;
         }
         break;

      case YXML_ATTRSTART:
         if (attr)
            attr = attr->next = (struct rxml_attrib_node*)calloc(1, sizeof(*attr));
         else
            attr = node->attrib = (struct rxml_attrib_node*)calloc(1, sizeof(*attr));

         if (attr->attrib)
            free(attr->attrib);
         attr->attrib = strdup(x.attr);

         valptr = buf->val;
         break;

      case YXML_ATTRVAL:
         for(c = 0; c < sizeof(x.data) && x.data[c]; ++c) {
            *valptr = x.data[c];
            ++valptr;
         }
         break;

      case YXML_ATTREND:
         if (valptr > buf->val) {
            *valptr = '\0';

            if (attr->value)
               free(attr->value);
            attr->value = strdup(buf->val);

            valptr = buf->val;
         }
         break;

      default:
         break;
      }
   }

   free(buf);
   return doc;

error:
   rxml_free_document(doc);
   free(buf);
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

const char *rxml_node_attrib(struct rxml_node *node, const char *attrib)
{
   struct rxml_attrib_node *attribs = NULL;
   for (attribs = node->attrib; attribs; attribs = attribs->next)
   {
      if (string_is_equal(attrib, attribs->attrib))
         return attribs->value;
   }

   return NULL;
}

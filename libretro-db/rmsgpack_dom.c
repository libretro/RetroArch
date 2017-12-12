/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmsgpack_dom.c).
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

#include "rmsgpack_dom.h"

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "rmsgpack.h"

#define MAX_DEPTH 128

struct dom_reader_state
{
	int i;
	struct rmsgpack_dom_value *stack[MAX_DEPTH];
};

static struct rmsgpack_dom_value *dom_reader_state_pop(struct dom_reader_state *s)
{
	struct rmsgpack_dom_value *v = s->stack[s->i];
	s->i--;
	return v;
}

static int dom_reader_state_push(struct dom_reader_state *s, struct rmsgpack_dom_value *v)
{
	if ((s->i + 1) == MAX_DEPTH)
		return -ENOMEM;
	s->i++;
	s->stack[s->i] = v;
	return 0;
}

static int dom_read_nil(void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
   v->type = RDT_NULL;
   return 0;
}

static int dom_read_bool(int value, void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);

   v->type = RDT_BOOL;
   v->val.bool_ = value;
   return 0;
}

static int dom_read_int(int64_t value, void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);

   v->type = RDT_INT;
   v->val.int_ = value;
   return 0;
}

static int dom_read_uint(uint64_t value, void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);

   v->type = RDT_UINT;
   v->val.uint_ = value;
   return 0;
}

static int dom_read_string(char *value, uint32_t len, void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);

   v->type = RDT_STRING;
   v->val.string.len = len;
   v->val.string.buff = value;
   return 0;
}

static int dom_read_bin(void *value, uint32_t len, void *data)
{
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v =
      (struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);

   v->type = RDT_BINARY;
   v->val.binary.len = len;
   v->val.binary.buff = (char *)value;
   return 0;
}

static int dom_read_map_start(uint32_t len, void *data)
{
   unsigned i;
   struct rmsgpack_dom_pair *items = NULL;
   struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
   struct rmsgpack_dom_value *v = dom_reader_state_pop(dom_state);

   v->type = RDT_MAP;
   v->val.map.len = len;
   v->val.map.items = NULL;

   items = (struct rmsgpack_dom_pair *)calloc(len,
         sizeof(struct rmsgpack_dom_pair));

   if (!items)
      return -ENOMEM;

   v->val.map.items = items;

   for (i = 0; i < len; i++)
   {
      if (dom_reader_state_push(dom_state, &items[i].value) < 0)
         return -ENOMEM;
      if (dom_reader_state_push(dom_state, &items[i].key) < 0)
         return -ENOMEM;
   }

   return 0;
}

static int dom_read_array_start(uint32_t len, void *data)
{
	unsigned i;
	struct dom_reader_state *dom_state = (struct dom_reader_state *)data;
	struct rmsgpack_dom_value *v       = dom_reader_state_pop(dom_state);
	struct rmsgpack_dom_value *items   = NULL;

	v->type = RDT_ARRAY;
	v->val.array.len = len;
	v->val.array.items = NULL;

	items = (struct rmsgpack_dom_value *)calloc(len, sizeof(*items));

	if (!items)
		return -ENOMEM;

	v->val.array.items = items;

	for (i = 0; i < len; i++)
   {
      if (dom_reader_state_push(dom_state, &items[i]) < 0)
         return -ENOMEM;
   }

	return 0;
}

static struct rmsgpack_read_callbacks dom_reader_callbacks = {
	dom_read_nil,
	dom_read_bool,
	dom_read_int,
	dom_read_uint,
	dom_read_string,
	dom_read_bin,
	dom_read_map_start,
	dom_read_array_start
};

void rmsgpack_dom_value_free(struct rmsgpack_dom_value *v)
{
   unsigned i;

   switch (v->type)
   {
      case RDT_STRING:
         free(v->val.string.buff);
         break;
      case RDT_BINARY:
         free(v->val.binary.buff);
         break;
      case RDT_MAP:
         for (i = 0; i < v->val.map.len; i++)
         {
            rmsgpack_dom_value_free(&v->val.map.items[i].key);
            rmsgpack_dom_value_free(&v->val.map.items[i].value);
         }
         free(v->val.map.items);
         break;
      case RDT_ARRAY:
         for (i = 0; i < v->val.array.len; i++)
            rmsgpack_dom_value_free(&v->val.array.items[i]);
         free(v->val.array.items);
         break;
      case RDT_NULL:
      case RDT_INT:
      case RDT_BOOL:
      case RDT_UINT:
         /* Do nothing */
         break;
   }
}

struct rmsgpack_dom_value *rmsgpack_dom_value_map_value(
      const struct rmsgpack_dom_value *map,
      const struct rmsgpack_dom_value *key)
{
   unsigned i;
   if (map->type != RDT_MAP)
      return NULL;

   for (i = 0; i < map->val.map.len; i++)
   {
      if (rmsgpack_dom_value_cmp(key, &map->val.map.items[i].key) == 0)
         return &map->val.map.items[i].value;
   }
   return NULL;
}

int rmsgpack_dom_value_cmp(
      const struct rmsgpack_dom_value *a,
      const struct rmsgpack_dom_value *b
)
{
   int rv;
   unsigned i;

   if (a == b)
      return 1;

   if (a->type != b->type)
      return 1;

   switch (a->type)
   {
      case RDT_NULL:
         return 0;
      case RDT_BOOL:
         return a->val.bool_ == b->val.bool_ ? 0 : 1;
      case RDT_INT:
         return a->val.int_ == b->val.int_ ? 0 : 1;
      case RDT_UINT:
         return a->val.uint_ == b->val.uint_ ? 0 : 1;
      case RDT_STRING:
         if (a->val.string.len != b->val.string.len)
            return 1;
         return strncmp(a->val.string.buff, b->val.string.buff, a->val.string.len);
      case RDT_BINARY:
         if (a->val.binary.len != b->val.binary.len)
            return 1;
         return memcmp(a->val.binary.buff, b->val.binary.buff, a->val.binary.len);
      case RDT_MAP:
         if (a->val.map.len != b->val.map.len)
            return 1;
         for (i = 0; i < a->val.map.len; i++)
         {
            if ((rv = rmsgpack_dom_value_cmp(&a->val.map.items[i].key,
                        &b->val.map.items[i].key)) != 0)
               return rv;
            if ((rv = rmsgpack_dom_value_cmp(&a->val.map.items[i].value,
                        &b->val.map.items[i].value)) != 0)
               return rv;
         }
         break;
      case RDT_ARRAY:
         if (a->val.array.len != b->val.array.len)
            return 1;
         for (i = 0; i < a->val.array.len; i++)
         {
            if ((rv = rmsgpack_dom_value_cmp(&a->val.array.items[i],
                        &b->val.array.items[i])) != 0)
               return rv;
         }
         break;
   }

   return 1;
}

void rmsgpack_dom_value_print(struct rmsgpack_dom_value *obj)
{
   unsigned i;

   switch (obj->type)
   {
      case RDT_NULL:
         printf("nil");
         break;
      case RDT_BOOL:
         if (obj->val.bool_)
            printf("true");
         else
            printf("false");
         break;
      case RDT_INT:
         printf("%" PRId64, (int64_t)obj->val.int_);
         break;
      case RDT_UINT:
         printf("%" PRIu64, (uint64_t)obj->val.uint_);
         break;
      case RDT_STRING:
         printf("\"%s\"", obj->val.string.buff);
         break;
      case RDT_BINARY:
         printf("\"");
         for (i = 0; i < obj->val.binary.len; i++)
            printf("%02X", (unsigned char) obj->val.binary.buff[i]);
         printf("\"");
         break;
      case RDT_MAP:
         printf("{");
         for (i = 0; i < obj->val.map.len; i++)
         {
            rmsgpack_dom_value_print(&obj->val.map.items[i].key);
            printf(": ");
            rmsgpack_dom_value_print(&obj->val.map.items[i].value);
            if (i < (obj->val.map.len - 1))
               printf(", ");
         }
         printf("}");
         break;
      case RDT_ARRAY:
         printf("[");
         for (i = 0; i < obj->val.array.len; i++)
         {
            rmsgpack_dom_value_print(&obj->val.array.items[i]);
            if (i < (obj->val.array.len - 1))
               printf(", ");
         }
         printf("]");
   }
}
int rmsgpack_dom_write(RFILE *fd, const struct rmsgpack_dom_value *obj)
{
   unsigned i;
   int rv = 0;
   int written = 0;

   switch (obj->type)
   {
      case RDT_NULL:
         return rmsgpack_write_nil(fd);
      case RDT_BOOL:
         return rmsgpack_write_bool(fd, obj->val.bool_);
      case RDT_INT:
         return rmsgpack_write_int(fd, obj->val.int_);
      case RDT_UINT:
         return rmsgpack_write_uint(fd, obj->val.uint_);
      case RDT_STRING:
         return rmsgpack_write_string(fd, obj->val.string.buff, obj->val.string.len);
      case RDT_BINARY:
         return rmsgpack_write_bin(fd, obj->val.binary.buff, obj->val.binary.len);
      case RDT_MAP:
         if ((rv = rmsgpack_write_map_header(fd, obj->val.map.len)) < 0)
            return rv;
         written += rv;

         for (i = 0; i < obj->val.map.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fd, &obj->val.map.items[i].key)) < 0)
               return rv;
            written += rv;
            if ((rv = rmsgpack_dom_write(fd, &obj->val.map.items[i].value)) < 0)
               return rv;
            written += rv;
         }
         break;
      case RDT_ARRAY:
         if ((rv = rmsgpack_write_array_header(fd, obj->val.array.len)) < 0)
            return rv;
         written += rv;

         for (i = 0; i < obj->val.array.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fd, &obj->val.array.items[i])) < 0)
               return rv;
            written += rv;
         }
   }
   return written;
}

int rmsgpack_dom_read(RFILE *fd, struct rmsgpack_dom_value *out)
{
   struct dom_reader_state s;
   int rv = 0;

   s.i        = 0;
   s.stack[0] = out;

   rv = rmsgpack_read(fd, &dom_reader_callbacks, &s);

   if (rv < 0)
      rmsgpack_dom_value_free(out);

   return rv;
}

int rmsgpack_dom_read_into(RFILE *fd, ...)
{
   va_list ap;
   struct rmsgpack_dom_value map;
   int rv;
   const char *key_name;
   struct rmsgpack_dom_value key;
   struct rmsgpack_dom_value *value;
   int64_t *int_value;
   uint64_t *uint_value;
   int *bool_value;
   char *buff_value;
   uint64_t min_len;
   int value_type = 0;

   va_start(ap, fd);

   rv = rmsgpack_dom_read(fd, &map);

   (void)value_type;

   if (rv < 0)
   {
      va_end(ap);
      return rv;
   }

   if (map.type != RDT_MAP)
      goto clean;

   while (1)
   {
      key_name = va_arg(ap, const char *);

      if (!key_name)
         goto clean;

      key.type            = RDT_STRING;
      key.val.string.len  = (uint32_t)strlen(key_name);
      key.val.string.buff = (char *) key_name;

      value = rmsgpack_dom_value_map_value(&map, &key);

      switch (value->type)
      {
         case RDT_INT:
            int_value   = va_arg(ap, int64_t *);
            *int_value  = value->val.int_;
            break;
         case RDT_BOOL:
            bool_value  = va_arg(ap, int *);
            *bool_value = value->val.bool_;
            break;
         case RDT_UINT:
            uint_value  = va_arg(ap, uint64_t *);
            *uint_value = value->val.uint_;
            break;
         case RDT_BINARY:
            buff_value  = va_arg(ap, char *);
            uint_value  = va_arg(ap, uint64_t *);
            *uint_value = value->val.binary.len;
            min_len     = (value->val.binary.len > *uint_value) ?
               *uint_value : value->val.binary.len;

            memcpy(buff_value, value->val.binary.buff, (size_t)min_len);
            break;
         case RDT_STRING:
            buff_value = va_arg(ap, char *);
            uint_value = va_arg(ap, uint64_t *);
            min_len    = (value->val.string.len + 1 > *uint_value) ?
               *uint_value : value->val.string.len + 1;
            *uint_value = min_len;

            memcpy(buff_value, value->val.string.buff, (size_t)min_len);
            break;
         default:
            goto clean;
      }
   }

clean:
   va_end(ap);
   rmsgpack_dom_value_free(&map);
   return 0;
}

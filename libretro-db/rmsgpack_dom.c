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

static struct rmsgpack_dom_value *dom_reader_state_pop(
      struct dom_reader_state *s)
{
   struct rmsgpack_dom_value *v = s->stack[s->i];
   s->i--;
   return v;
}

static void puts_i64(int64_t dec)
{
   unsigned i;
   signed   j;
   signed char digits[19 + 1] = {0}; /* max i64:  9,223,372,036,854,775,807 */
   uint64_t decimal           = (dec < 0) ? (uint64_t)-dec : (uint64_t)+dec;

   digits[19] = '\0';

   for (j = sizeof(digits) - 2; j >= 0; j--)
   {
      digits[j] = decimal % 10;
      decimal /= 10;
   }

   for (i = 0; i < sizeof(digits) - 1; i++)
      digits[i] += '0';

   for (i = 0; i < sizeof(digits) - 2; i++)
      if (digits[i] != '0')
         break; /* Don't print leading zeros to the console. */

   if (dec < 0)
      putchar('-');
   fputs((char *)&digits[i], stdout);
}

static void puts_u64(uint64_t decimal)
{
   signed   j;
   unsigned i;
   char digits[20 + 1] = {0}; /* max u64:  18,446,744,073,709,551,616 */

   for (j = sizeof(digits) - 2; j >= 0; j--)
   {
      digits[j] = decimal % 10;
      decimal /= 10;
   }

   for (i = 0; i < sizeof(digits) - 1; i++)
      digits[i] += '0';
   for (i = 0; i < sizeof(digits) - 2; i++)
      if (digits[i] != '0')
         break; /* Don't print leading zeros to the console. */

   fputs(&digits[i], stdout);
}

static int dom_reader_state_push(struct dom_reader_state *s,
      struct rmsgpack_dom_value *v)
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

   v->type            = RDT_ARRAY;
   v->val.array.len   = len;
   v->val.array.items = NULL;

   items          = (struct rmsgpack_dom_value *)calloc(len, sizeof(struct rmsgpack_dom_pair));

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
         v->val.string.buff = NULL;
         break;
      case RDT_BINARY:
         free(v->val.binary.buff);
         v->val.binary.buff = NULL;
         break;
      case RDT_MAP:
         for (i = 0; i < v->val.map.len; i++)
         {
            rmsgpack_dom_value_free(&v->val.map.items[i].key);
            rmsgpack_dom_value_free(&v->val.map.items[i].value);
         }
         free(v->val.map.items);
         v->val.map.items = NULL;
         break;
      case RDT_ARRAY:
         for (i = 0; i < v->val.array.len; i++)
            rmsgpack_dom_value_free(&v->val.array.items[i]);
         free(v->val.array.items);
         v->val.array.items = NULL;
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
         return (a->val.bool_ == b->val.bool_) ? 0 : 1;
      case RDT_INT:
         return (a->val.int_ == b->val.int_) ? 0 : 1;
      case RDT_UINT:
         return (a->val.uint_ == b->val.uint_) ? 0 : 1;
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
         puts_i64(obj->val.int_);
         break;
      case RDT_UINT:
         puts_u64(obj->val.uint_);
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
int rmsgpack_dom_write(FILE *fp, const struct rmsgpack_dom_value *obj)
{
   unsigned i;
   int rv      = 0;
   int written = 0;

   switch (obj->type)
   {
      case RDT_NULL:
         return rmsgpack_write_nil(fp);
      case RDT_BOOL:
         return rmsgpack_write_bool(fp, obj->val.bool_);
      case RDT_INT:
         return rmsgpack_write_int(fp, obj->val.int_);
      case RDT_UINT:
         return rmsgpack_write_uint(fp, obj->val.uint_);
      case RDT_STRING:
         return rmsgpack_write_string(fp, obj->val.string.buff, obj->val.string.len);
      case RDT_BINARY:
         return rmsgpack_write_bin(fp, obj->val.binary.buff, obj->val.binary.len);
      case RDT_MAP:
         if ((rv = rmsgpack_write_map_header(fp, obj->val.map.len)) < 0)
            return rv;
         written += rv;

         for (i = 0; i < obj->val.map.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fp, &obj->val.map.items[i].key)) < 0)
               return rv;
            written += rv;
            if ((rv = rmsgpack_dom_write(fp, &obj->val.map.items[i].value)) < 0)
               return rv;
            written += rv;
         }
         break;
      case RDT_ARRAY:
         if ((rv = rmsgpack_write_array_header(fp, obj->val.array.len)) < 0)
            return rv;
         written += rv;

         for (i = 0; i < obj->val.array.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fp, &obj->val.array.items[i])) < 0)
               return rv;
            written += rv;
         }
   }
   return written;
}

int rmsgpack_dom_read(FILE *fp, struct rmsgpack_dom_value *out)
{
   struct dom_reader_state s = {0};
   int rv                    = 0;

   s.stack[0] = out;

   rv = rmsgpack_read(fp, &dom_reader_callbacks, &s);

   if (rv < 0)
      rmsgpack_dom_value_free(out);

   return rv;
}

int rmsgpack_dom_read_into(FILE *fp, ...)
{
   va_list ap;
   int rv;
   struct rmsgpack_dom_value map;
   struct rmsgpack_dom_value key;
   struct rmsgpack_dom_value *value;
   int64_t *int_value;
   uint64_t *uint_value;
   int *bool_value;
   uint64_t min_len;
   char *buff_value                 = NULL;
   const char *key_name             = NULL;
   int value_type                   = 0;

   va_start(ap, fp);

   rv = rmsgpack_dom_read(fp, &map);

   (void)value_type;

   if (rv < 0)
   {
      va_end(ap);
      return rv;
   }

   if (map.type != RDT_MAP)
   {
      rv = -EINVAL;
      goto clean;
   }

   while (1)
   {
      key_name = va_arg(ap, const char *);

      if (!key_name)
      {
         rv = 0;
         goto clean;
      }

      key.type        = RDT_STRING;
      key.val.string.len  = strlen(key_name);
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
            rv = -1;
            goto clean;
      }
   }

clean:
   va_end(ap);
   rmsgpack_dom_value_free(&map);
   return 0;
}

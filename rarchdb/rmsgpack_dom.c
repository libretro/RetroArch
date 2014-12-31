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
	struct rmsgpack_dom_value* stack[MAX_DEPTH];
};


static struct rmsgpack_dom_value *dom_reader_state_pop(struct dom_reader_state *s)
{
	struct rmsgpack_dom_value *v = s->stack[s->i];
	s->i--;
	return v;
}

static int dom_reader_state_push(struct dom_reader_state *s, struct rmsgpack_dom_value *v)
{
	//TODO: Verify we don't overflow
	s->i++;
	s->stack[s->i] = v;
	return 0;
}

static int dom_read_nil(void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_NULL;
	return 0;
}

static int dom_read_bool(int value, void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_BOOL;
	v->bool_ = value;
	return 0;
}

static int dom_read_int(int64_t value, void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_INT;
	v->int_ = value;
	return 0;
}

static int dom_read_uint(uint64_t value, void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_UINT;
	v->uint_ = value;
	return 0;
}

static int dom_read_string(char* value, uint32_t len, void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_STRING;
	v->string.len = len;
	v->string.buff = value;
	return 0;
}

static int dom_read_bin(void* value, uint32_t len, void *data)
{
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	struct rmsgpack_dom_value *v = 
		(struct rmsgpack_dom_value*)dom_reader_state_pop(dom_state);
	v->type = RDT_BINARY;
	v->binary.len = len;
	v->binary.buff = (char*)value;
	return 0;
}

static int dom_read_map_start(uint32_t len, void *data)
{
   int i;
   struct rmsgpack_dom_pair *items = NULL;
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
	
   struct rmsgpack_dom_value *v = dom_reader_state_pop(dom_state);
	v->type = RDT_MAP;
	v->map.len = len;
	v->map.items = NULL;

	items = (struct rmsgpack_dom_pair *)calloc(len, sizeof(struct rmsgpack_dom_pair));

	if (!items)
		return -ENOMEM;

	v->map.items = items;
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
   int i;
	struct dom_reader_state *dom_state = (struct dom_reader_state*)data;
   struct rmsgpack_dom_value *v = dom_reader_state_pop(dom_state);
   struct rmsgpack_dom_value *items = NULL;
   v->type = RDT_ARRAY;
   v->array.len = len;
   v->array.items = NULL;

   items = (struct rmsgpack_dom_value*)calloc(len, sizeof(struct rmsgpack_dom_pair));

   if (!items)
      return -ENOMEM;

   v->array.items = items;

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
   int i;

   switch (v->type)
   {
      case RDT_STRING:
         free(v->string.buff);
         break;
      case RDT_BINARY:
         free(v->binary.buff);
         break;
      case RDT_MAP:
         for (i = 0; i < v->map.len; i++)
         {
            rmsgpack_dom_value_free(&v->map.items[i].key);
            rmsgpack_dom_value_free(&v->map.items[i].value);
         }
         free(v->map.items);
         break;
      case RDT_ARRAY:
         for (i = 0; i < v->array.len; i++)
            rmsgpack_dom_value_free(&v->array.items[i]);
         free(v->array.items);
         break;
      case RDT_NULL:
      case RDT_INT:
      case RDT_BOOL:
      case RDT_UINT:
         // Do nothing
         break;
   }
}

struct rmsgpack_dom_value *rmsgpack_dom_value_map_value(
      const struct rmsgpack_dom_value *map, const struct rmsgpack_dom_value *key)
{
   int i;
   if (map->type != RDT_MAP)
      return NULL;

   for (i = 0; i < map->map.len; i++)
   {
      if (rmsgpack_dom_value_cmp(key, &map->map.items[i].key) == 0)
         return &map->map.items[i].value;
   }
   return NULL;
}

int rmsgpack_dom_value_cmp(
      const struct rmsgpack_dom_value *a, const struct rmsgpack_dom_value *b)
{
   int rv, i;
   if (a == b)
      return 1;

   if (a->type != b->type)
      return 1;

   switch (a->type)
   {
      case RDT_NULL:
         return 0;
      case RDT_BOOL:
         return a->bool_ == b->bool_ ? 1 : 0;
      case RDT_INT:
         return a->int_ == b->int_ ? 1 : 0;
      case RDT_UINT:
         return a->uint_ == b->uint_ ? 1 : 0;
      case RDT_STRING:
         if (a->string.len != b->string.len)
            return 1;
         return strncmp(a->string.buff, b->string.buff, a->string.len);
      case RDT_BINARY:
         if (a->binary.len != b->binary.len)
            return 1;
         return memcmp(a->binary.buff, b->binary.buff, a->binary.len);
      case RDT_MAP:
         if (a->map.len != b->map.len)
            return 1;
         for (i = 0; i < a->map.len; i++)
         {
            if((rv = rmsgpack_dom_value_cmp(&a->map.items[i].key, &b->map.items[i].key)) != 0)
               return rv;
            if((rv = rmsgpack_dom_value_cmp(&a->map.items[i].value, &b->map.items[i].value)) != 0)
               return rv;
         }
         break;
      case RDT_ARRAY:
         if (a->array.len != b->array.len)
            return 1;
         for (i = 0; i < a->array.len; i++)
         {
            if((rv = rmsgpack_dom_value_cmp(&a->array.items[i], &b->array.items[i])) != 0)
               return rv;
         }
         break;
   }

   return 1;
}

void rmsgpack_dom_value_print(struct rmsgpack_dom_value *obj)
{
   int i;

   switch (obj->type)
   {
      case RDT_NULL:
         printf("nil");
         break;
      case RDT_BOOL:
         if (obj->bool_)
            printf("true");
         else
            printf("false");
         break;
      case RDT_INT:
#ifdef _WIN32
         printf("%I64d", (signed long long)obj->int_);
#else
         printf("%lld", (signed long long)obj->int_);
#endif
         break;
      case RDT_UINT:
#ifdef _WIN32
         printf("%I64u", (unsigned long long)obj->uint_);
#else
         printf("%llu",  (unsigned long long)obj->uint_);
#endif
         break;
      case RDT_STRING:
         printf("\"%s\"", obj->string.buff);
         break;
      case RDT_BINARY:
         printf("\"");
         for (i = 0; i < obj->binary.len; i++)
            printf("%02X", (unsigned char) obj->binary.buff[i]);
         printf("\"");
         break;
      case RDT_MAP:
         printf("{");
         for (i = 0; i < obj->map.len; i++)
         {
            rmsgpack_dom_value_print(&obj->map.items[i].key);
            printf(": ");
            rmsgpack_dom_value_print(&obj->map.items[i].value);
            if (i < (obj->map.len - 1))
               printf(", ");
         }
         printf("}");
         break;
      case RDT_ARRAY:
         printf("[");
         for (i = 0; i < obj->array.len; i++)
         {
            rmsgpack_dom_value_print(&obj->array.items[i]);
            if (i < (obj->array.len - 1))
               printf(", ");
         }
         printf("]");
   }
}
int rmsgpack_dom_write(int fd, const struct rmsgpack_dom_value *obj)
{
   int i;
   int rv = 0;
   int written = 0;

   switch (obj->type)
   {
      case RDT_NULL:
         return rmsgpack_write_nil(fd);
      case RDT_BOOL:
         return rmsgpack_write_bool(fd, obj->bool_);
      case RDT_INT:
         return rmsgpack_write_int(fd, obj->int_);
      case RDT_UINT:
         return rmsgpack_write_uint(fd, obj->uint_);
      case RDT_STRING:
         return rmsgpack_write_string(fd, obj->string.buff, obj->string.len);
      case RDT_BINARY:
         return rmsgpack_write_bin(fd, obj->binary.buff, obj->binary.len);
      case RDT_MAP:
         if ((rv = rmsgpack_write_map_header(fd, obj->map.len)) < 0)
            return rv;
         written += rv;
         for (i = 0; i < obj->map.len; i++)
         {
            if((rv = rmsgpack_dom_write(fd, &obj->map.items[i].key)) < 0)
               return rv;
            written += rv;
            if((rv = rmsgpack_dom_write(fd, &obj->map.items[i].value)) < 0)
               return rv;
            written += rv;
         }
         break;
      case RDT_ARRAY:
         if ((rv = rmsgpack_write_array_header(fd, obj->array.len)) < 0)
            return rv;
         written += rv;
         for (i = 0; i < obj->array.len; i++)
         {
            if((rv = rmsgpack_dom_write(fd, &obj->array.items[i])) < 0)
               return rv;
            written += rv;
         }
   }
   return written;
}

int rmsgpack_dom_read(int fd, struct rmsgpack_dom_value *out)
{
   int rv = 0;
   struct dom_reader_state s;
   s.i = 0;
   s.stack[0] = out;
   rv = rmsgpack_read(fd, &dom_reader_callbacks, &s);

   if (rv < 0)
      rmsgpack_dom_value_free(out);

   return rv;
}

int rmsgpack_dom_read_into(int fd, ...)
{
   va_list ap;
   struct rmsgpack_dom_value map;
   int rv;
   const char* key_name;
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
      return rv;

   if (map.type != RDT_MAP)
   {
      rv = -EINVAL;
      goto clean;
   }

   while (1)
   {
      key_name = va_arg(ap, const char*);
      if (key_name == NULL)
      {
         rv = 0;
         goto clean;
      }

      key.type = RDT_STRING;
      key.string.len = strlen(key_name);
      key.string.buff = (char *) key_name;

      value = rmsgpack_dom_value_map_value(&map, &key);

      switch(value->type)
      {
         case RDT_INT:
            int_value = va_arg(ap, int64_t*);
            *int_value = value->int_;
            break;
         case RDT_BOOL:
            bool_value = va_arg(ap, int*);
            *bool_value = value->bool_;
            break;
         case RDT_UINT:
            uint_value = va_arg(ap, uint64_t*);
            *uint_value = value->uint_;
            break;
         case RDT_BINARY:
            buff_value = va_arg(ap, char*);
            uint_value = va_arg(ap, uint64_t*);
            *uint_value = value->binary.len;
            min_len = value->binary.len > *uint_value ? *uint_value : value->binary.len;
            memcpy(
                  buff_value,
                  value->binary.buff,
                  min_len
                  );

            break;
         case RDT_STRING:
            buff_value = va_arg(ap, char*);
            uint_value = va_arg(ap, uint64_t*);
            min_len = value->string.len + 1 > *uint_value ? *uint_value : value->string.len + 1;
            *uint_value = min_len;
            memcpy(
                  buff_value,
                  value->string.buff,
                  min_len
                  );
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

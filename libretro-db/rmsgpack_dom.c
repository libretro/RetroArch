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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "rmsgpack.h"

/* For the simple stack-based reader state */
#define MAX_DEPTH 128

struct rmsgpack_dom_reader_state
{
   int i;
   bool growable;
   int capacity;
   struct rmsgpack_dom_value **stack;
};

static struct rmsgpack_dom_value *rmsgpack_dom_reader_state_pop(
      struct rmsgpack_dom_reader_state *s)
{
   struct rmsgpack_dom_value *v = s->stack[s->i];
   s->i--;
   return v;
}

static int rmsgpack_dom_reader_state_push(
      struct rmsgpack_dom_reader_state *s, struct rmsgpack_dom_value *v)
{
   if ((s->i + 1) == s->capacity)
   {
      size_t new_capacity;
      struct rmsgpack_dom_value **new_stack;

      if (!s->growable)
         return -1;

      /* realloc-to-tmp: pre-patch was 's->stack = realloc(s->stack,
       * ...)' which on OOM would leak the old buffer (self-assign
       * drops the only reference).  Also pre-patch did
       * 'capacity *= 2' before the realloc, so on failure the
       * capacity claimed the new size while the allocation was
       * still the old size - out-of-sync state that would manifest
       * as a heap-buffer-overflow later if anything else ignored
       * the -1 return.  Compute new_capacity into a local, realloc
       * into a tmp, commit both only on success. */
      new_capacity = s->capacity * 2;
      new_stack    = (struct rmsgpack_dom_value **)
         realloc(s->stack, new_capacity * sizeof(struct rmsgpack_dom_value *));
      if (!new_stack)
         return -1;
      s->stack    = new_stack;
      s->capacity = (int)new_capacity;
   }
   s->i++;
   s->stack[s->i] = v;
   return 0;
}

static int dom_read_nil(void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       =
      (struct rmsgpack_dom_value*)rmsgpack_dom_reader_state_pop(dom_state);
   v->type                            = RDT_NULL;
   return 0;
}

static int dom_read_bool(int value, void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       =
      (struct rmsgpack_dom_value*)rmsgpack_dom_reader_state_pop(dom_state);
   v->type                            = RDT_BOOL;
   v->val.bool_                       = value;
   return 0;
}

static int dom_read_int(int64_t value, void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       =
      (struct rmsgpack_dom_value*)rmsgpack_dom_reader_state_pop(dom_state);
   v->type                            = RDT_INT;
   v->val.int_                        = value;
   return 0;
}

static int dom_read_uint(uint64_t value, void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       =
      (struct rmsgpack_dom_value*)rmsgpack_dom_reader_state_pop(dom_state);
   v->type                            = RDT_UINT;
   v->val.uint_                       = value;
   return 0;
}

static int dom_read_string(char *value, uint32_t len, void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       =
      (struct rmsgpack_dom_value*)rmsgpack_dom_reader_state_pop(dom_state);

   v->type                            = RDT_STRING;
   v->val.string.len                  = len;
   v->val.string.buff                 = value;
   return 0;
}

static int dom_read_bin(void *value, uint32_t len, void *data)
{
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       = (struct rmsgpack_dom_value*)
      rmsgpack_dom_reader_state_pop(dom_state);
   v->type                            = RDT_BINARY;
   v->val.binary.len                  = len;
   v->val.binary.buff                 = (char *)value;
   return 0;
}

static int dom_read_map_start(uint32_t len, void *data)
{
   unsigned i;
   struct rmsgpack_dom_pair    *items = NULL;
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value       *v = rmsgpack_dom_reader_state_pop(dom_state);

   v->type                            = RDT_MAP;
   v->val.map.len                     = 0;
   v->val.map.items                   = NULL;

   /* An empty map is legal MsgPack, and calloc(0, n) is permitted to
    * return NULL, which the old code could not tell from failure. */
   if (len == 0)
      return 0;

   /* 'len' is only published once storage backs it.  It used to be
    * assigned before the calloc(), so a failed allocation left the
    * value as an RDT_MAP of len pairs with a NULL items pointer.  The
    * error then propagates to rmsgpack_dom_read_with(), whose cleanup
    * calls rmsgpack_dom_value_free() - which walks items[0..len) and
    * dereferences NULL.
    *
    * The allocation is attacker-influenced: len is bounded only by
    * half the bytes remaining in the file, so a large .rdb with a
    * MAP32 header sized to exceed the process' remaining memory
    * reaches it.  Reproduced with a 2 MB .rdb claiming 1000000 pairs
    * under a constrained allocator:
    *
    *   AddressSanitizer: SEGV on unknown address 0x000000000010
    *     #0 rmsgpack_dom_value_free  rmsgpack_dom.c:204
    *     #1 rmsgpack_dom_value_free  rmsgpack_dom.c:219
    *     #2 rmsgpack_dom_read_with   rmsgpack_dom.c:473 */
   if (!(items = (struct rmsgpack_dom_pair *)
      calloc(len, sizeof(struct rmsgpack_dom_pair))))
      return -1;

   v->val.map.items                   = items;
   v->val.map.len                     = len;

   for (i = 0; i < len; i++)
   {
      if (rmsgpack_dom_reader_state_push(dom_state, &items[i].value) < 0)
         return -1;
      if (rmsgpack_dom_reader_state_push(dom_state, &items[i].key) < 0)
         return -1;
   }

   return 0;
}

static int dom_read_array_start(uint32_t len, void *data)
{
   size_t i;
   struct rmsgpack_dom_reader_state *dom_state = (struct rmsgpack_dom_reader_state *)data;
   struct rmsgpack_dom_value *v       = rmsgpack_dom_reader_state_pop(dom_state);
   struct rmsgpack_dom_value *items   = NULL;

   v->type                            = RDT_ARRAY;
   v->val.array.len                   = 0;
   v->val.array.items                 = NULL;

   /* See dom_read_map_start(): empty arrays are legal, and 'len' must
    * not be published until storage backs it or the cleanup path
    * walks a NULL items pointer. */
   if (len == 0)
      return 0;

   if (!(items = (struct rmsgpack_dom_value *)
            calloc(len, sizeof(*items))))
      return -1;

   v->val.array.items                 = items;
   v->val.array.len                   = len;

   for (i = 0; i < len; i++)
   {
      if (rmsgpack_dom_reader_state_push(dom_state, &items[len-i-1]) < 0)
         return -1;
   }

   return 0;
}

void rmsgpack_dom_value_free(struct rmsgpack_dom_value *v)
{
   size_t i;
   switch (v->type)
   {
      case RDT_STRING:
         free(v->val.string.buff);
         v->val.string.buff = NULL;
         v->val.string.len  = 0;
         break;
      case RDT_BINARY:
         free(v->val.binary.buff);
         v->val.binary.buff = NULL;
         v->val.binary.len  = 0;
         break;
      case RDT_MAP:
         for (i = 0; i < v->val.map.len; i++)
         {
            rmsgpack_dom_value_free(&v->val.map.items[i].key);
            rmsgpack_dom_value_free(&v->val.map.items[i].value);
         }
         free(v->val.map.items);
         v->val.map.items = NULL;
         v->val.map.len   = 0;
         break;
      case RDT_ARRAY:
         for (i = 0; i < v->val.array.len; i++)
            rmsgpack_dom_value_free(&v->val.array.items[i]);
         free(v->val.array.items);
         v->val.array.items = NULL;
         v->val.array.len   = 0;
         break;
      case RDT_NULL:
      case RDT_INT:
      case RDT_BOOL:
      case RDT_UINT:
         /* Do nothing */
         break;
   }

   /* Leave the value in the same state a fresh RDT_NULL has, so that
    * a second free of the same object - which the reader below can
    * ask for when a record fails to parse - is a no-op rather than a
    * double free.  libretrodb_create() already open-coded this
    * assignment after every call; it belongs here. */
   v->type = RDT_NULL;
}

struct rmsgpack_dom_value *rmsgpack_dom_value_map_value(
      const struct rmsgpack_dom_value *map,
      const struct rmsgpack_dom_value *key)
{
   if (map->type == RDT_MAP)
   {
      unsigned i;
      for (i = 0; i < map->val.map.len; i++)
      {
         if (rmsgpack_dom_value_cmp(key, &map->val.map.items[i].key) == 0)
            return &map->val.map.items[i].value;
      }
   }
   return NULL;
}

int rmsgpack_dom_value_cmp(
      const struct rmsgpack_dom_value *a,
      const struct rmsgpack_dom_value *b)
{
   if ((a != b) && (a->type == b->type))
   {
      switch (a->type)
      {
         case RDT_NULL:
            return 0;
         case RDT_BOOL:
            if (a->val.bool_ == b->val.bool_)
               return 0;
            break;
         case RDT_INT:
            if (a->val.int_ == b->val.int_)
               return 0;
            break;
         case RDT_UINT:
            if (a->val.uint_ == b->val.uint_)
               return 0;
            break;
         case RDT_STRING:
            if (a->val.string.len == b->val.string.len)
               return strncmp(a->val.string.buff,
                     b->val.string.buff, a->val.string.len);
            break;
         case RDT_BINARY:
            if (a->val.binary.len == b->val.binary.len)
               return memcmp(a->val.binary.buff,
                     b->val.binary.buff, a->val.binary.len);
            break;
         case RDT_MAP:
            if (a->val.map.len == b->val.map.len)
            {
               unsigned i;
               for (i = 0; i < a->val.map.len; i++)
               {
                  int rv;
                  if ((rv = rmsgpack_dom_value_cmp(&a->val.map.items[i].key,
                              &b->val.map.items[i].key)) != 0)
                     return rv;
                  if ((rv = rmsgpack_dom_value_cmp(&a->val.map.items[i].value,
                              &b->val.map.items[i].value)) != 0)
                     return rv;
               }
            }
            break;
         case RDT_ARRAY:
            if (a->val.array.len == b->val.array.len)
            {
               unsigned i;
               for (i = 0; i < a->val.array.len; i++)
               {
                  int rv;
                  if ((rv = rmsgpack_dom_value_cmp(&a->val.array.items[i],
                              &b->val.array.items[i])) != 0)
                     return rv;
               }
            }
            break;
      }
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

int rmsgpack_dom_write(intfstream_t *fd, const struct rmsgpack_dom_value *obj)
{
   unsigned i;
   int rv   = 0;
   int _len = 0;

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
         _len += rv;

         for (i = 0; i < obj->val.map.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fd, &obj->val.map.items[i].key)) < 0)
               return rv;
            _len += rv;
            if ((rv = rmsgpack_dom_write(fd, &obj->val.map.items[i].value)) < 0)
               return rv;
            _len += rv;
         }
         break;
      case RDT_ARRAY:
         if ((rv = rmsgpack_write_array_header(fd, obj->val.array.len)) < 0)
            return rv;
         _len += rv;

         for (i = 0; i < obj->val.array.len; i++)
         {
            if ((rv = rmsgpack_dom_write(fd, &obj->val.array.items[i])) < 0)
               return rv;
            _len += rv;
         }
   }
   return _len;
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

int rmsgpack_dom_read_with(intfstream_t *fd, struct rmsgpack_dom_value *out, struct rmsgpack_dom_reader_state *s)
{
   int rv;

   /* 'out' is uninitialised on entry - every caller passes the
    * address of a bare automatic (database_cursor_iterate(),
    * database_info_list_new_names_only(), and the fast path in
    * libretrodb_cursor_read_item()).  rmsgpack_read() only writes
    * through it once a callback fires, so a stream that fails before
    * the first callback left the error path below freeing whatever
    * happened to be in that stack slot.
    *
    * In the cursor loop that slot is reused across iterations and
    * still holds the previous record: type RDT_MAP with an 'items'
    * pointer that was already freed.  Reading a truncated .rdb
    * therefore ran rmsgpack_dom_value_free() over a dangling map -
    * a use-after-free walking freed pairs, then a double free of
    * the pair array itself.
    *
    * Claim the object before handing it to the reader so the error
    * path always has a well-defined value to release. */
   out->type   = RDT_NULL;

   s->i        = 0;
   s->stack[0] = out;
   if ((rv = rmsgpack_read(fd, &dom_reader_callbacks, s)) < 0)
      rmsgpack_dom_value_free(out);
   return rv;
}

struct rmsgpack_dom_reader_state *rmsgpack_dom_reader_state_new(void)
{
   struct rmsgpack_dom_reader_state *s = (struct rmsgpack_dom_reader_state *)calloc(1,
         sizeof(struct rmsgpack_dom_reader_state));
   /* NULL-check: the field writes below NULL-deref on OOM. */
   if (!s)
      return NULL;
   s->i        = 0;
   s->capacity = 1024;
   s->growable = true;
   s->stack    = (struct rmsgpack_dom_value **)calloc(1024, sizeof(struct rmsgpack_dom_value *));
   /* NULL-check the stack calloc: consumers (rmsgpack_dom_read
    * above at line ~422 writes s->stack[0] unconditionally) would
    * NULL-deref on OOM.  Free the containing state struct and
    * return NULL so the caller (rmsgpack_dom_read does this via
    * its own caller chain) can fall through gracefully. */
   if (!s->stack)
   {
      free(s);
      return NULL;
   }
   return s;
}

void rmsgpack_dom_reader_state_free(struct rmsgpack_dom_reader_state *state)
{
   /* NULL-check: rmsgpack_dom_reader_state_new can now return
    * NULL on OOM (this commit).  Callers that defer cleanup to
    * the end of a function (e.g. input/bsv/bsvmovie.c's
    * decompress path at line ~1833) may hit this with a NULL
    * state pointer if the reader_state_new call failed. */
   if (!state)
      return;
   free(state->stack);
   free(state);
}

int rmsgpack_dom_read(intfstream_t *fd, struct rmsgpack_dom_value *out)
{
   struct rmsgpack_dom_reader_state s;
   s.i        = 0;
   s.growable = false;
   s.capacity = MAX_DEPTH;
   s.stack    = (struct rmsgpack_dom_value **)alloca(MAX_DEPTH*sizeof(struct rmsgpack_dom_value *));
   return rmsgpack_dom_read_with(fd, out, &s);
}

/**
 * rmsgpack_dom_read_into:
 *
 * Read a map from @fd and extract the requested keys into
 * caller-supplied storage.  Varargs are triples of
 *
 *    const char *key, enum rmsgpack_dom_field_type type, <out...>
 *
 * terminated by a NULL key.  RDF_UINT / RDF_INT / RDF_BOOL take one
 * output pointer; RDF_STRING / RDF_BINARY take a buffer plus a
 * uint64_t* holding the buffer capacity on entry and the number of
 * bytes written on exit.
 *
 * The type argument is what makes this safe.  This function used to
 * switch on the type tag found in the *file* and consume a different
 * number of va_args per case - one for RDT_UINT, two for RDT_STRING
 * and RDT_BINARY.  A .rdb that declared a key with an unexpected
 * type therefore slid the whole va_list out of step, and every
 * subsequent key wrote a file-controlled uint64 through a pointer
 * belonging to a different field.  libretrodb_find_index() passes
 * five outputs, so the desync there is an arbitrary write rather
 * than merely a crash.
 *
 * A missing key was equally unsafe: rmsgpack_dom_value_map_value()
 * returns NULL and the result went straight into "switch
 * (value->type)".  Reproduced with a 26-byte .rdb whose metadata map
 * is keyed "kount" instead of "count":
 *
 *   rmsgpack_dom.c:526: member access within null pointer
 *   #1 libretrodb_open  libretrodb.c:248
 *
 * Reading the caller's expectation first fixes both: the va_list
 * advances by a fixed amount per key regardless of what the file
 * says, an absent key leaves the output untouched, and a key whose
 * stored type disagrees with the requested one is a parse failure.
 *
 * Returns: 0 on success, -1 if the stream is not a map, if a
 * requested key is absent, or if a key's stored type does not match
 * the requested type.
 */
int rmsgpack_dom_read_into(intfstream_t *fd, ...)
{
   int rv = 0;
   va_list ap;
   struct rmsgpack_dom_value map;
   struct rmsgpack_dom_value key;

   va_start(ap, fd);

   if ((rv = rmsgpack_dom_read(fd, &map)) < 0)
   {
      va_end(ap);
      return rv;
   }

   if (map.type != RDT_MAP)
   {
      va_end(ap);
      rmsgpack_dom_value_free(&map);
      return -1;
   }

   for (;;)
   {
      struct rmsgpack_dom_value *value;
      enum rmsgpack_dom_field_type ftype;
      char       *buff_value;
      uint64_t   *uint_value;
      uint64_t    capacity;
      uint64_t    min_len;
      const char *key_name = va_arg(ap, const char *);

      if (!key_name)
         break;

      /* Read the caller's expected type before touching the file's,
       * so the number of va_args consumed for this key is fixed. */
      ftype                = (enum rmsgpack_dom_field_type)
         va_arg(ap, int);

      key.type             = RDT_STRING;
      key.val.string.len   = (uint32_t)strlen(key_name);
      key.val.string.buff  = (char *)key_name;

      value                = rmsgpack_dom_value_map_value(&map, &key);

      switch (ftype)
      {
         case RDF_INT:
            {
               int64_t *int_value = va_arg(ap, int64_t *);
               /* MsgPack has no single canonical encoding for a small
                * integer - a positive fixint decodes as RDT_INT while
                * rmsgpack_write_uint() emits UINT8..UINT64, which
                * decode as RDT_UINT.  Accept either spelling as long
                * as the value is representable, so that requiring a
                * type does not reject a validly encoded file. */
               if (value && value->type == RDT_INT)
                  *int_value = value->val.int_;
               else if (   value
                        && value->type == RDT_UINT
                        && value->val.uint_ <= (uint64_t)INT64_MAX)
                  *int_value = (int64_t)value->val.uint_;
               else
                  rv = -1;
            }
            break;
         case RDF_BOOL:
            {
               int *bool_value = va_arg(ap, int *);
               if (!value || value->type != RDT_BOOL)
               {
                  rv = -1;
                  break;
               }
               *bool_value = value->val.bool_;
            }
            break;
         case RDF_UINT:
            {
               uint64_t *u = va_arg(ap, uint64_t *);
               /* See RDF_INT above: a non-negative RDT_INT is a valid
                * encoding of a small unsigned value. */
               if (value && value->type == RDT_UINT)
                  *u = value->val.uint_;
               else if (   value
                        && value->type == RDT_INT
                        && value->val.int_ >= 0)
                  *u = (uint64_t)value->val.int_;
               else
                  rv = -1;
            }
            break;
         case RDF_BINARY:
            buff_value     = va_arg(ap, char *);
            uint_value     = va_arg(ap, uint64_t *);
            if (!value || value->type != RDT_BINARY)
            {
               rv          = -1;
               break;
            }
            /* *uint_value is the caller's buffer capacity on entry
             * and the number of bytes copied on exit.  Compute the
             * clamped copy length BEFORE overwriting *uint_value,
             * otherwise the bound check collapses to len > len and
             * the memcpy can overrun the caller's buffer with
             * attacker-controlled length from the db file. */
            capacity       = *uint_value;
            min_len        = (value->val.binary.len > capacity)
               ? capacity : value->val.binary.len;
            *uint_value    = min_len;
            memcpy(buff_value, value->val.binary.buff, (size_t)min_len);
            break;
         case RDF_STRING:
            buff_value     = va_arg(ap, char *);
            uint_value     = va_arg(ap, uint64_t *);
            if (!value || value->type != RDT_STRING)
            {
               rv          = -1;
               break;
            }
            /* Cast to uint64_t before adding 1 to avoid uint32_t
             * overflow when string.len == UINT32_MAX, which would
             * wrap the sum to 0 and collapse the bounds check. */
            capacity       = *uint_value;
            min_len        = ((uint64_t)value->val.string.len + 1 > capacity)
               ? capacity : (uint64_t)value->val.string.len + 1;
            *uint_value    = min_len;
            memcpy(buff_value, value->val.string.buff, (size_t)min_len);
            /* memcpy above may have stopped short of the terminator
             * when the stored string does not fit.  Callers treat
             * this as a C string (libretrodb_find_index() calls
             * strlen() on idx->name), so terminate unconditionally
             * rather than handing back 50 unterminated bytes. */
            if (min_len > 0)
               buff_value[min_len - 1] = '\0';
            break;
         default:
            rv             = -1;
            break;
      }

      if (rv < 0)
         break;
   }

   va_end(ap);
   rmsgpack_dom_value_free(&map);
   return rv;
}

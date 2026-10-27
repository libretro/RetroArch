/* rmsgpack_read() takes a value from a window the stream lends where
 * it can, and reads what the window does not hold. This writes a
 * stream of values - maps, arrays, integers of every width and sign,
 * strings and binaries from empty to well over the window - and reads
 * it back through a buffered stream with a window smaller than some of
 * them, a file stream, and a memory stream, which lends nothing. Every
 * value must come back as written, and after each the stream must
 * stand where the value ends. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/interface_stream.h>
#include <streams/file_stream.h>

#include "../../rmsgpack.h"
#include "../../rmsgpack_dom.h"

#define N_VALUES 400
#define BIG      (100 * 1024)

static int     failures = 0;
static uint8_t big[BIG];

/* A value whose shape depends on @i */
static void write_value(intfstream_t *fd, unsigned i)
{
   switch (i % 7)
   {
      case 0:
         rmsgpack_write_map_header(fd, 3);
         rmsgpack_write_string(fd, "name", 4);
         rmsgpack_write_string(fd, (const char*)big, i % 300);
         rmsgpack_write_string(fd, "size", 4);
         rmsgpack_write_uint(fd, (uint64_t)i * 0x10001u);
         rmsgpack_write_string(fd, "crc", 3);
         rmsgpack_write_bin(fd, big + i, 4);
         break;
      case 1:
         rmsgpack_write_int(fd, -(int64_t)i);
         break;
      case 2:
         rmsgpack_write_int(fd, -((int64_t)1 << (i % 63)));
         break;
      case 3:
         /* longer than the buffered stream's window, and than the
          * reader's own */
         rmsgpack_write_string(fd, (const char*)big, (uint32_t)(i * 997 % BIG));
         break;
      case 4:
         rmsgpack_write_array_header(fd, 3);
         rmsgpack_write_uint(fd, (uint64_t)1 << (i % 64));
         rmsgpack_write_bool(fd, (int)(i & 1));
         rmsgpack_write_nil(fd);
         break;
      case 5:
         rmsgpack_write_bin(fd, big, (uint32_t)(i * 131 % 70000));
         break;
      default:
         rmsgpack_write_uint(fd, i);
         break;
   }
}

/* A map's value by its key */
static const struct rmsgpack_dom_value *field(
      const struct rmsgpack_dom_value *map, const char *key)
{
   struct rmsgpack_dom_value k;
   if (map->type != RDT_MAP)
      return NULL;
   k.type            = RDT_STRING;
   k.val.string.len  = (uint32_t)strlen(key);
   k.val.string.buff = (char*)key;
   return rmsgpack_dom_value_map_value(map, &k);
}

static int64_t expect_int(unsigned i)
{
   return i % 7 == 1 ? -(int64_t)i : -((int64_t)1 << (i % 63));
}

static void check_value(const struct rmsgpack_dom_value *v, unsigned i,
      const char *how)
{
   int ok = 0;
   switch (i % 7)
   {
      case 0:
      {
         const struct rmsgpack_dom_value *name = field(v, "name");
         const struct rmsgpack_dom_value *size = field(v, "size");
         const struct rmsgpack_dom_value *crc  = field(v, "crc");
         ok = v->type == RDT_MAP && v->val.map.len == 3
            && name && name->type == RDT_STRING
            && name->val.string.len == i % 300
            && !memcmp(name->val.string.buff, big, i % 300)
            /* a small unsigned number is written as a fixint, and read
             * back as signed */
            && size && (size->type == RDT_UINT || size->type == RDT_INT)
            && size->val.uint_ == (uint64_t)i * 0x10001u
            && crc && crc->type == RDT_BINARY && crc->val.binary.len == 4
            && !memcmp(crc->val.binary.buff, big + i, 4);
         break;
      }
      case 1:
      case 2:
         ok = (v->type == RDT_INT || v->type == RDT_UINT)
            && v->val.int_ == expect_int(i);
         break;
      case 3:
         ok = v->type == RDT_STRING
            && v->val.string.len == i * 997 % BIG
            && !memcmp(v->val.string.buff, big, v->val.string.len);
         break;
      case 4:
         ok = v->type == RDT_ARRAY && v->val.array.len == 3
            && v->val.array.items[0].val.uint_ == (uint64_t)1 << (i % 64)
            && v->val.array.items[1].type == RDT_BOOL
            && v->val.array.items[1].val.bool_ == (int)(i & 1)
            && v->val.array.items[2].type == RDT_NULL;
         break;
      case 5:
         ok = v->type == RDT_BINARY
            && v->val.binary.len == i * 131 % 70000
            && !memcmp(v->val.binary.buff, big, v->val.binary.len);
         break;
      default:
         ok = (v->type == RDT_UINT || v->type == RDT_INT) && v->val.uint_ == i;
         break;
   }
   if (!ok)
   {
      fprintf(stderr, "FAIL: %s: value %u (shape %u)\n", how, i, i % 7);
      failures++;
   }
}

static void read_back(intfstream_t *fd, const int64_t *ends, const char *how)
{
   unsigned i;
   if (!fd)
   {
      fprintf(stderr, "FAIL: %s: cannot open\n", how);
      failures++;
      return;
   }
   for (i = 0; i < N_VALUES; i++)
   {
      struct rmsgpack_dom_value v;
      if (rmsgpack_dom_read(fd, &v) < 0)
      {
         fprintf(stderr, "FAIL: %s: value %u did not read\n", how, i);
         failures++;
         break;
      }
      check_value(&v, i, how);
      rmsgpack_dom_value_free(&v);
      if (intfstream_tell(fd) != ends[i])
      {
         fprintf(stderr, "FAIL: %s: after value %u at %lld, not %lld\n", how, i,
               (long long)intfstream_tell(fd), (long long)ends[i]);
         failures++;
         break;
      }
   }
   intfstream_close(fd);
   free(fd);
}

int main(void)
{
   static int64_t ends[N_VALUES];
   const char    *path = "rmsgpack_window_test.bin";
   intfstream_t  *fd;
   uint8_t       *mem;
   int64_t        size;
   unsigned       i;

   for (i = 0; i < BIG; i++)
      big[i] = (uint8_t)(i * 7 + (i >> 11));

   if (!(fd = intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return 2;
   for (i = 0; i < N_VALUES; i++)
   {
      write_value(fd, i);
      ends[i] = intfstream_tell(fd);
   }
   size = ends[N_VALUES - 1];
   intfstream_close(fd);
   free(fd);

   /* a window smaller than the long values, and one smaller still */
   read_back(intfstream_open_buffered(path, 4096), ends, "buffered, 4 KiB window");
   read_back(intfstream_open_buffered(path, 64), ends, "buffered, 64 octet window");
   read_back(intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE), ends, "file");
   read_back(intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS), ends, "file, mapped");

   /* a memory stream lends nothing: every value is read */
   if ((mem = (uint8_t*)malloc((size_t)size)))
   {
      FILE *f = fopen(path, "rb");
      if (f && fread(mem, 1, (size_t)size, f) == (size_t)size)
         read_back(intfstream_open_memory(mem, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE, (uint64_t)size), ends, "memory");
      else
      {
         fprintf(stderr, "FAIL: cannot read the file back\n");
         failures++;
      }
      if (f)
         fclose(f);
      free(mem);
   }
   remove(path);

   if (failures)
   {
      fprintf(stderr, "FAIL: %d checks\n", failures);
      return 1;
   }
   printf("ok: %u values through buffered, file, mapped and memory streams\n",
         N_VALUES);
   return 0;
}

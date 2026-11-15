#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/file_path.h>
#include "../../../deps/rcheevos/src/rhash/rc_hash_internal.h"
#include "../../../deps/rcheevos/include/rc_consoles.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"rhash FAIL %d: %s\n",__LINE__,#c); failures++; } } while (0)
static void put(uint8_t *p, uint64_t value, unsigned bytes)
{
   unsigned i;
   for (i = 0; i < bytes; i++) { p[i] = (uint8_t)value; value >>= 8; }
}

int rhash_primitives_test(const char *dir)
{
   unsigned unit, offset, length, i;
   uint8_t digest[256];
   char hex[515], expected[3];
   md5_state_t md5;
   rc_hash_iterator_t iterator;
   for (unit = 2; unit <= 4; unit += 2)
      for (offset = 0; offset < 8; offset++)
         for (length = 0; length < 68; length++)
         {
            uint8_t *base = (uint8_t *)malloc(length + offset + (length + offset == 0));
            uint8_t *buffer;
            CHECK(base != NULL);
            if (!base) return failures;
            memset(base, 0xa5, length + offset);
            buffer = base + offset;
            for (i = 0; i < length; i++) buffer[i] = (uint8_t)(i * 17);
            if (unit == 2) rc_hash_byteswap16(buffer, buffer + length);
            else rc_hash_byteswap32(buffer, buffer + length);
            for (i = 0; i < offset; i++) CHECK(base[i] == 0xa5);
            for (i = 0; i < length; i++)
            {
               unsigned from = i - i % unit + unit <= length ? i ^ (unit - 1) : i;
               CHECK(buffer[i] == (uint8_t)(from * 17));
            }
            free(base);
         }
   for (i = 0; i < 256; i++) digest[i] = (uint8_t)i;
   memset(hex, '!', sizeof(hex));
   hash_digest_to_hex(hex + 1, digest, 256);
   CHECK(hex[0] == '!' && hex[513] == 0 && hex[514] == '!');
   for (i = 0; i < 256; i++)
   {
      snprintf(expected, sizeof(expected), "%02x", i);
      CHECK(!memcmp(hex + 1 + 2 * i, expected, 2));
   }
   hash_digest_to_hex(hex, NULL, 0);
   CHECK(hex[0] == 0);
   memset(&iterator, 0, sizeof(iterator));
   md5_init(&md5); md5_append(&md5, (const uint8_t *)"abc", 3);
   CHECK(rc_hash_finalize(&iterator, &md5, hex));
   CHECK(!strcmp(hex, "900150983cd24fb0d6963f7d28e17f72"));
   for (i = 0; i < 2; i++)
   {
      static const char name[] = "DATA\\File.TXT";
      uint8_t zip[128];
      char path[1024];
      size_t cdir = 46 + sizeof(name) - 1, size;
      FILE *file;
      memset(zip, 0, sizeof(zip));
      put(zip, 0x02014b50, 4);
      put(zip + 4, 20, 2); put(zip + 6, 20, 2);
      put(zip + 16, 0x352441c2, 4);
      put(zip + 20, 3, 4); put(zip + 24, i ? UINT32_MAX : 3, 4);
      put(zip + 28, sizeof(name) - 1, 2);
      memcpy(zip + 46, name, sizeof(name) - 1);
      if (i)
      {
         put(zip + 30, 20, 2);
         put(zip + 42, UINT32_MAX, 4);
         put(zip + cdir, 1, 2); put(zip + cdir + 2, 16, 2);
         put(zip + cdir + 4, 3, 8); put(zip + cdir + 12, 0, 8);
         cdir += 20;
      }
      put(zip + cdir, 0x06054b50, 4);
      put(zip + cdir + 8, 1, 2); put(zip + cdir + 10, 1, 2);
      put(zip + cdir + 12, cdir, 4);
      size = cdir + 22;
      fill_pathname_join_special(path, dir, "rhash_primitives.dosz", sizeof(path));
      file = fopen(path, "wb"); CHECK(file != NULL);
      if (!file) return failures;
      CHECK(fwrite(zip, 1, size, file) == size); fclose(file);
      CHECK(rc_hash_generate_from_file(hex, RC_CONSOLE_MS_DOS, path));
      CHECK(!strcmp(hex, "f6d75afd9192612b4ee487be0b21b94b"));
      CHECK(remove(path) == 0);
   }
   printf("rhash primitives: %s\n", failures ? "FAILED" : "passed");
   return failures;
}

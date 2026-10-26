/* filestream_read_file() on the descriptor path with the Wii U's read
 * alignment (VFS_DESCRIPTOR_READ_ALIGN 64, HAVE_WIIU_DESCRIPTOR_IO):
 * the whole file arrives in one read() into a line-aligned buffer that
 * asks for whole lines, so the SD devoptab reads it in place in one FS
 * call. read() here is a stand-in that checks each request and then
 * does what a DMA read does to the lines it covers past the bytes it
 * delivers: their contents are lost. ASan sees any of that land
 * outside the buffer; the content and terminator checks see it land
 * on the file's bytes. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <streams/file_stream.h>

#define LINE 64

static int    failures;
static int    reads;
static size_t last_len;
static int    misaligned;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

#include "wiiu_descriptor_read_stub.h"

/* vfs_implementation.c's read() calls come here (see the Makefile). */
#ifdef _WIN32
int test_read(int fd, void *buf, unsigned int len)
#else
ssize_t test_read(int fd, void *buf, size_t len)
#endif
{
   long got;
   reads++;
   last_len = len;
#ifndef _WIN32
   /* Windows has no memalign whose block free() takes, so the
    * alignment is the Linux build's to check. */
   if ((uintptr_t)buf & (LINE - 1))
      misaligned++;
#endif
   if (len & (LINE - 1))
      misaligned++;
   got = (long)read(fd, buf, (unsigned)len);
   if (got >= 0 && (size_t)got < len)
      memset((uint8_t*)buf + got, 0xEE, len - (size_t)got);
   return got;
}

static void case_size(size_t size)
{
   char     path[64];
   FILE    *f;
   size_t   i;
   uint8_t *want = (uint8_t*)malloc(size ? size : 1);
   void    *buf  = NULL;
   int64_t  len  = 0;

   snprintf(path, sizeof(path), "wiiu_descriptor_read_%u.tmp", (unsigned)size);
   for (i = 0; i < size; i++)
      want[i] = (uint8_t)(i * 13 + 7);
   f = fopen(path, "wb");
   fwrite(want, 1, size, f);
   fclose(f);

   reads = 0; misaligned = 0;
   CHECK(filestream_read_file(path, &buf, &len) == 1);
   CHECK(len == (int64_t)size);
   CHECK(reads == 1);
   CHECK(misaligned == 0);
   CHECK(last_len >= size && last_len < size + LINE);
   if (buf)
   {
      CHECK(memcmp(buf, want, size) == 0);
      CHECK(((char*)buf)[size] == '\0');
   }
   if (failures)
      fprintf(stderr, "  (file of %u bytes)\n", (unsigned)size);
   free(buf);
   free(want);
   remove(path);
}

int main(void)
{
   static const size_t sizes[] = { 1, 63, 64, 65, 100, 1000, 4096, 4097,
         70000, 131072, 131073, 262145 };
   size_t i;
   for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
      case_size(sizes[i]);
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_descriptor_read: ok\n");
   return 0;
}

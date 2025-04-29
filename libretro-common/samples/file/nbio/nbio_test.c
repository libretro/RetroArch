#include <stdio.h>
#include <string.h>

#include <file/nbio.h>

static void nbio_write_test(void)
{
   size_t _len;
   bool looped = false;
   void *ptr   = NULL;
   struct nbio_t* write = nbio_open("test.bin", NBIO_WRITE);
   if (!write)
      puts("[ERROR]: nbio_open failed (1)");

   nbio_resize(write, 1024*1024);

   ptr = nbio_get_ptr(write, &_len);
   if (_len != 1024*1024)
      puts("[ERROR]: wrong size (1)");

   memset(ptr, 0x42, 1024*1024);
   nbio_begin_write(write);

   while (!nbio_iterate(write))
      looped=true;

   if (!looped)
      puts("[SUCCESS]: Write finished immediately.");

   nbio_free(write);
}

static void nbio_read_test(void)
{
   size_t _len;
   bool looped = false;
   struct nbio_t* read = nbio_open("test.bin", NBIO_READ);
   void* ptr           = nbio_get_ptr(read, &_len);
   if (!read)
      puts("[ERROR]: nbio_open failed (2)");

   if (_len != 1024*1024)
      puts("[ERROR]: wrong size (2)");
   if (ptr)
      puts("[SUCCESS]: Read pointer is available before iterating.");

   nbio_begin_read(read);

   while (!nbio_iterate(read))
      looped=true;

   if (!looped)
      puts("[SUCCESS]: Read finished immediately.");

   ptr = nbio_get_ptr(read, &_len);

   if (_len != 1024*1024)
      puts("[ERROR]: wrong size (3)");
   if (*(char*)ptr != 0x42 || memcmp(ptr, (char*)ptr+1, 1024*1024-1))
      puts("[ERROR]: wrong data");

   nbio_free(read);
}

int main(void)
{
   nbio_write_test();
   nbio_read_test();
}

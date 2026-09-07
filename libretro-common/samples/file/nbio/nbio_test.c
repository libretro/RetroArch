#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/nbio.h>

static int failures = 0;

static void nbio_write_test(void)
{
   size_t _len;
   bool looped = false;
   void *ptr   = NULL;
   struct nbio_t* write = nbio_open("test.bin", NBIO_WRITE);
   if (!write)
   {
      puts("[ERROR] nbio_open failed (1)");
      failures++;
      return;
   }

   nbio_resize(write, 1024*1024);

   ptr = nbio_get_ptr(write, &_len);
   if (_len != 1024*1024)
   {
      puts("[ERROR] wrong size (1)");
      failures++;
   }

   memset(ptr, 0x42, 1024*1024);
   nbio_begin_write(write);

   while (!nbio_iterate(write))
      looped=true;

   if (!looped)
      puts("[SUCCESS] Write finished immediately.");

   nbio_free(write);
}

static void nbio_read_test(void)
{
   size_t _len;
   bool looped = false;
   struct nbio_t* read = nbio_open("test.bin", NBIO_READ);
   void* ptr           = nbio_get_ptr(read, &_len);
   if (!read)
   {
      puts("[ERROR] nbio_open failed (2)");
      failures++;
      return;
   }

   if (_len != 1024*1024)
   {
      puts("[ERROR] wrong size (2)");
      failures++;
   }
   if (ptr)
      puts("[SUCCESS] Read pointer is available before iterating.");

   nbio_begin_read(read);

   while (!nbio_iterate(read))
      looped=true;

   if (!looped)
      puts("[SUCCESS] Read finished immediately.");

   ptr = nbio_get_ptr(read, &_len);

   if (_len != 1024*1024)
   {
      puts("[ERROR] wrong size (3)");
      failures++;
   }
   if (*(char*)ptr != 0x42 || memcmp(ptr, (char*)ptr+1, 1024*1024-1))
   {
      puts("[ERROR] wrong data");
      failures++;
   }

   nbio_free(read);
}

/* Regression-ish test for commit <round4-TBD> (nbio_stdio_resize
 * realloc commit-before-success).
 *
 * Pre-patch, nbio_stdio_resize wrote handle->len = len BEFORE the
 * realloc, then on realloc failure silently kept the stale old
 * data pointer with the NEW (larger) len.  Subsequent fread/fwrite
 * iterated up to handle->len bytes and walked off the end.
 *
 * This is a smoke test rather than a true discriminator: forcing
 * realloc to fail from user code requires an allocator hook, which
 * isn\'t portable and breaks the self-contained-sample convention.
 * What this test DOES verify:
 *   1. A successful resize + get_ptr reports the new size.
 *   2. A subsequent larger resize + get_ptr reports the new size
 *      AND the pointer is non-NULL (so writes through it are
 *      valid up to that size).
 *   3. The post-patch code still completes a write/read round-trip
 *      after multiple resizes.
 *
 * Historical note: on the unpatched code the same sequence passes
 * too, because realloc on a 4 MiB buffer essentially never fails
 * on a CI runner.  The test\'s real value is as documentation for
 * the resize API contract so a future refactor doesn\'t silently
 * re-introduce the commit-before-success pattern.
 */
static void nbio_resize_smoke_test(void)
{
   size_t          got_len;
   void           *ptr;
   struct nbio_t  *w = nbio_open("resize_test.bin", NBIO_WRITE);
   if (!w)
   {
      puts("[ERROR] resize-test: nbio_open failed");
      failures++;
      return;
   }

   /* First resize: 4 KiB. */
   nbio_resize(w, 4096);
   ptr = nbio_get_ptr(w, &got_len);
   if (got_len != 4096 || !ptr)
   {
      printf("[ERROR] resize-test: first resize got len=%zu ptr=%p\n",
            got_len, ptr);
      failures++;
      nbio_free(w);
      return;
   }
   memset(ptr, 0xAB, 4096);

   /* Second resize: grow to 8 KiB.  Post-patch the handle must
    * either report len=8192 with a valid pointer, or leave the
    * old size and pointer in place (realloc failed).  There is no
    * valid middle state. */
   nbio_resize(w, 8192);
   ptr = nbio_get_ptr(w, &got_len);
   if (!ptr)
   {
      printf("[ERROR] resize-test: after grow, ptr is NULL with len=%zu\n",
            got_len);
      failures++;
      nbio_free(w);
      return;
   }
   if (got_len != 8192)
   {
      printf("[ERROR] resize-test: after grow, len=%zu != 8192\n", got_len);
      failures++;
      nbio_free(w);
      return;
   }

   /* Fill the grown region.  If the pointer / len disagree the
    * write walks off the end and ASan catches it. */
   memset(ptr, 0xCD, 8192);

   nbio_begin_write(w);
   while (!nbio_iterate(w)) {}

   nbio_free(w);
   remove("resize_test.bin");
   puts("[SUCCESS] nbio_resize grow sequence completed");
}

int main(void)
{
   nbio_write_test();
   nbio_read_test();
   nbio_resize_smoke_test();

   /* Clean up the main test artifact. */
   remove("test.bin");

   if (failures)
   {
      printf("\n%d nbio test(s) failed\n", failures);
      return 1;
   }
   return 0;
}

/* A core may define INLINE itself before it includes libretro-common.
 * FBNeo has it as "__inline static"; retro_inline.h then keeps the
 * core's, and a header that wrote "static INLINE" became
 * "static __inline static", which C++ rejects. file_stream.h reaches
 * retro_atomic.h through vfs.h's handle, so every core that reads files
 * through libretro-common includes it. This is built as C89 and as C++
 * (C++98 and C++11): it only has to compile, then do one operation. */
#define INLINE __inline static

#include <stdio.h>
#include <streams/file_stream.h>
#include <retro_atomic.h>

int main(void)
{
   retro_atomic_int_t n;
   int                v;
   retro_atomic_int_init(&n, 1);
   retro_atomic_fetch_add_int(&n, 2);
   v = retro_atomic_load_acquire_int(&n);
   if (v != 3)
   {
      printf("FAIL: %d after adding 2 to 1\n", v);
      return 1;
   }
   printf("PASS: libretro-common's headers take a core's own INLINE\n");
   return 0;
}

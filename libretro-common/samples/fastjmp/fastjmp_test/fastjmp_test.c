/* fastjmp_test.c - set, jump from deeper, and the registers are intact.
 *
 * 1. A round trip: fastjmp_set returns 0, a call three frames down does
 *    fastjmp_jmp with 7, fastjmp_set returns 7, and the frames between
 *    are gone.
 * 2. Callee-saved registers: values held in locals across the set, at
 *    -O2 with enough live values that the compiler must keep some in
 *    callee-saved registers, are the same after the jump. On a wrong
 *    register list this is the test that fails.
 * 3. Repeated: the same buffer is set and jumped to a hundred thousand
 *    times, which is what an interpreter's state-change loop does.
 * 4. Native or fallback is reported, so the log says which was run.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <fastjmp.h>

static fastjmp_buf g_buf;
static volatile int g_depth;

static void level3(int v)
{
   g_depth = 3;
   fastjmp_jmp(&g_buf, v);
   printf("  FAIL: returned past fastjmp_jmp\n");
}
static void level2(int v) { g_depth = 2; level3(v); }
static void level1(int v) { g_depth = 1; level2(v); }

/* Not inlined, so a, b, c... must survive a real call. */
static int __attribute__((noinline)) mix(int x) { return x * 3 + 1; }

int main(void)
{
   int ok = 1;
   int r;
   /* 2: eight independent live values across the set */
   uint64_t a = 0x1111111111111111ull, b = 0x2222222222222222ull, c = 0x3333333333333333ull,
            d = 0x4444444444444444ull, e = 0x5555555555555555ull, f = 0x6666666666666666ull,
            g = 0x7777777777777777ull, h = 0x8888888888888888ull;
   volatile uint64_t sink = 0;
   int i;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("fastjmp\n");
#if defined(FASTJMP_NATIVE)
   printf("  native implementation, buffer %u bytes\n", (unsigned)sizeof(fastjmp_buf));
#else
   printf("  setjmp fallback on this architecture\n");
#endif

   /* 1 */
   g_depth = 0;
   r = fastjmp_set(&g_buf);
   if (r == 0)
   {
      level1(7);
      printf("  FAIL: level1 returned\n"); ok = 0;
   }
   else
   {
      printf("  %s: fastjmp_set returned %d from depth %d\n", (r == 7 && g_depth == 3) ? "jump" : "FAIL", r, (int)g_depth);
      if (r != 7 || g_depth != 3) ok = 0;
   }

   /* 2: mix keeps the values live and non-constant across the set */
   a = mix((int)a); b = mix((int)b); c = mix((int)c); d = mix((int)d);
   e = mix((int)e); f = mix((int)f); g = mix((int)g); h = mix((int)h);
   {
      uint64_t sa = a, sb = b, sc = c, sd = d, se = e, sf = f, sg = g, sh = h;
      r = fastjmp_set(&g_buf);
      if (r == 0)
      {
         /* clobber every register the callee-saved set could hold by
          * doing real work in a deeper frame, then jump */
         for (i = 0; i < 64; i++) sink += mix(i);
         level1(9);
      }
      else
      {
         int same = (a == sa && b == sb && c == sc && d == sd && e == se && f == sf && g == sg && h == sh);
         printf("  %s: eight values held across the set survive the jump\n", same ? "registers" : "FAIL");
         if (!same) ok = 0;
      }
   }

   /* 3 */
   {
      volatile int n = 0;
      for (;;)
      {
         r = fastjmp_set(&g_buf);
         if (r == 0)
            level1(1);
         n++;
         if (n >= 100000)
            break;
      }
      printf("  loop: %d set/jump round trips\n", (int)n);
   }

   /* 4: a buffer the compiler is free to misplace. Win64 stores
    * xmm6-xmm15 with movaps, so the type itself has to ask for 16-byte
    * alignment; as the member after a char it otherwise sits at +1 and
    * the first fastjmp_set faults. A static on its own is no test of
    * this: compilers put large statics on a 16-byte boundary anyway. */
   {
      static struct { char pad; fastjmp_buf jb; } odd;
      volatile int hops = 0;
#if defined(FASTJMP_NATIVE) && defined(FASTJMP_X86_64) && defined(_WIN32)
      unsigned mis = (unsigned)((size_t)&odd.jb % 16);
      printf("  %s: a member after a char is at %u mod 16\n", mis ? "FAIL" : "aligned", mis);
      if (mis) ok = 0;
#endif
      r = fastjmp_set(&odd.jb);
      if (r == 0)
      {
         hops = 1;
         fastjmp_jmp(&odd.jb, 5);
      }
      printf("  %s: set and jump through a struct member\n", (r == 5 && hops == 1) ? "member" : "FAIL");
      if (r != 5 || hops != 1) ok = 0;
   }

   printf(ok ? "fastjmp: ok\n" : "fastjmp: FAILED\n");
   return ok ? 0 : 1;
}

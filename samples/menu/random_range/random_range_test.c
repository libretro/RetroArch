/* random_range() from libretro-common's retro_math.h, which the menu's
 * Scan uses to pick a row: every rand() value must land inside the
 * inclusive range asked for, the top of rand() included, and every
 * value of the range must come up about equally often.
 *
 * rand() is replaced by a stand-in that returns what the test sets,
 * so the whole rand() range near RAND_MAX can be walked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Built a second time as a 15-bit rand(), as MSVC and mingw have */
#ifdef TEST_RAND_MAX
#undef RAND_MAX
#define RAND_MAX TEST_RAND_MAX
#endif

static int fake_value = 0;
static int fake_rand(void) { return fake_value; }

#define rand fake_rand
#include <retro_math.h>
#undef rand

static unsigned failures = 0;

static void check_range(unsigned min, unsigned max)
{
   size_t r;
   int v;
   int lo = (RAND_MAX > 4096) ? RAND_MAX - 4096 : 0;

   fake_value = 0;
   r          = random_range(min, max);
   if (r != min)
   {
      printf("FAIL random_range(%u, %u) with rand() 0: %lu, not %u\n",
            min, max, (unsigned long)r, min);
      failures++;
   }

   /* The top of rand(): where a float scale rounds up to max + 1 */
   for (v = lo; ; v++)
   {
      fake_value = v;
      r          = random_range(min, max);
      if (r < min || r > max)
      {
         printf("FAIL random_range(%u, %u) with rand() %d: %lu\n",
               min, max, v, (unsigned long)r);
         failures++;
         break;
      }
      if (v == RAND_MAX)
         break;
   }

   /* The top of rand() reaches max itself when the range is no wider
    * than rand() is */
   fake_value = RAND_MAX;
   r          = random_range(min, max);
   if (     min != max && r != max
         && (unsigned long)(max - min) <= (unsigned long)RAND_MAX)
   {
      printf("FAIL random_range(%u, %u) with rand() RAND_MAX: %lu, not %u\n",
            min, max, (unsigned long)r, max);
      failures++;
   }
}

/* Every value of [0, n) about as often as any other over rand(). */
static void check_spread(unsigned n)
{
   unsigned long counts[8];
   unsigned long least = ULONG_MAX;
   unsigned long most  = 0;
   unsigned long steps = 0;
   unsigned long stride;
   unsigned long v;
   unsigned i;

   for (i = 0; i < n; i++)
      counts[i] = 0;
   stride = (unsigned long)RAND_MAX / 65536UL + 1UL;
   for (v = 0; v <= (unsigned long)RAND_MAX; v += stride)
   {
      size_t r;
      fake_value = (int)v;
      r          = random_range(0, n - 1);
      if (r >= n)
      {
         printf("FAIL spread %u: rand() %lu gave %lu\n",
               n, v, (unsigned long)r);
         failures++;
         return;
      }
      counts[r]++;
      steps++;
   }
   for (i = 0; i < n; i++)
   {
      if (counts[i] < least)
         least = counts[i];
      if (counts[i] > most)
         most = counts[i];
   }
   if (!least || most - least > steps / 100UL)
   {
      printf("FAIL spread %u: counts from %lu to %lu over %lu draws\n",
            n, least, most, steps);
      failures++;
   }
}

int main(void)
{
   check_range(0, 0);
   check_range(7, 7);
   check_range(0, 1);
   check_range(0, 3);
   check_range(0, 4);
   check_range(5, 9);
   check_range(0, 65535);
   check_range(0, UINT_MAX);
   check_spread(2);
   check_spread(3);
   check_spread(5);
   check_spread(8);

   if (failures)
   {
      printf("random_range_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS random_range_test\n");
   return 0;
}

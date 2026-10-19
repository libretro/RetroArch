/* The device numbers shared between the joypad driver and the sources
 * beside it (input/input_device_pool.h).
 *
 *   driver alone  -> its pads are on their own slots' numbers
 *   beside it     -> pads are given free numbers from the top down;
 *                    one that comes back gets its number again
 *   collision     -> the driver's pad takes its number; the pad beside
 *                    it is named to move and is given another
 *   full          -> no number, and nothing given twice
 *   churn         -> in thousands of mixed arrivals and departures no
 *                    number is held twice, the driver's pads are on
 *                    their slots, and a pad's number only changes when
 *                    it comes back or is moved by a collision */
#include <stdio.h>
#include <stdlib.h>

#include "input_device_pool.h"

static unsigned failures;
#define CHECK(cond, msg) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } } while (0)

static unsigned popcount16(unsigned v)
{
   unsigned n = 0;
   for (; v; v &= v - 1)
      n++;
   return n;
}

static void driver_alone(void)
{
   input_device_pool_t pool;
   unsigned i, src, slot;
   input_device_pool_init(&pool);
   for (i = 0; i < 4; i++)
      CHECK(!input_device_pool_driver_connect(&pool, i, &src, &slot),
            "a driver pad moved something with nothing beside it");
   CHECK(pool.driver == 0x000f && !pool.beside, "driver numbers are not its slots");
   for (i = 0; i < INPUT_POOL_DEVICES; i++)
      CHECK(!input_device_pool_owner(&pool, i, &src, &slot),
            "a number belongs to a source beside the driver");
   input_device_pool_driver_disconnect(&pool, 2);
   CHECK(pool.driver == 0x000b, "a driver pad's number stayed taken");
}

static void beside(void)
{
   input_device_pool_t pool;
   unsigned src, slot;
   int a, b, c;
   input_device_pool_init(&pool);
   input_device_pool_driver_connect(&pool, 0, &src, &slot);
   input_device_pool_driver_connect(&pool, 1, &src, &slot);

   a = input_device_pool_claim(&pool, 0, 0);
   b = input_device_pool_claim(&pool, 0, 1);
   CHECK(a == 15 && b == 14, "pads beside are not given the top numbers");
   CHECK(input_device_pool_claim(&pool, 0, 0) == 15,
         "claiming twice gave another number");
   CHECK(input_device_pool_owner(&pool, 14, &src, &slot)
         && src == 0 && slot == 1, "the owner of a number is wrong");

   /* gone and back: its number again, though a later pad came */
   input_device_pool_release(&pool, 0, 0);
   c = input_device_pool_claim(&pool, 1, 0);
   CHECK(c == 15, "a freed number was not the highest free");
   input_device_pool_release(&pool, 1, 0);
   CHECK(input_device_pool_claim(&pool, 0, 0) == 15,
         "a pad back did not get its number again");

   /* its number taken meanwhile: the highest free */
   input_device_pool_release(&pool, 0, 0);
   CHECK(input_device_pool_claim(&pool, 1, 3) == 15, "claim");
   CHECK(input_device_pool_claim(&pool, 0, 0) == 13,
         "a pad back on a taken number did not get the highest free");

   CHECK(input_device_pool_claim(&pool, INPUT_POOL_SOURCES, 0) < 0
         && input_device_pool_claim(&pool, 0, INPUT_POOL_SLOTS) < 0,
         "an unknown source or slot got a number");
}

static void collision(void)
{
   input_device_pool_t pool;
   unsigned i, src = 99, slot = 99;
   int n;
   input_device_pool_init(&pool);
   for (i = 1; i < INPUT_POOL_DEVICES; i++)
      input_device_pool_driver_connect(&pool, i, &src, &slot);
   n = input_device_pool_claim(&pool, 1, 5);
   CHECK(n == 0, "the one free number was not given");
   CHECK(input_device_pool_driver_connect(&pool, 0, &src, &slot)
         && src == 1 && slot == 5,
         "the driver's pad on a number beside it did not move it");
   CHECK(!input_device_pool_owner(&pool, 0, &src, &slot),
         "the moved pad still owns its number");
   CHECK(input_device_pool_claim(&pool, 1, 5) < 0,
         "a number was given when all are taken");
   input_device_pool_driver_disconnect(&pool, 9);
   CHECK(input_device_pool_claim(&pool, 1, 5) == 9,
         "the moved pad was not given the next free number");
}

static void churn(void)
{
   input_device_pool_t pool;
   int driver_on[INPUT_POOL_DEVICES];
   int num[INPUT_POOL_SOURCES][INPUT_POOL_SLOTS];
   unsigned step, i, s;
   input_device_pool_init(&pool);
   memset(driver_on, 0, sizeof(driver_on));
   memset(num, -1, sizeof(num));
   srand(1234);

   for (step = 0; step < 200000; step++)
   {
      unsigned r = (unsigned)rand();
      unsigned held = 0;
      if (r % 3 == 0)
      {
         /* the driver's pad: only low slots, as drivers fill from 0 */
         unsigned idx = (r >> 4) % 8;
         unsigned src, slot;
         if (driver_on[idx])
         {
            input_device_pool_driver_disconnect(&pool, idx);
            driver_on[idx] = 0;
         }
         else
         {
            if (input_device_pool_driver_connect(&pool, idx, &src, &slot))
            {
               CHECK(num[src][slot] == (int)idx, "moved the wrong pad");
               num[src][slot] = input_device_pool_claim(&pool, src, slot);
            }
            driver_on[idx] = 1;
         }
      }
      else
      {
         unsigned src  = (r >> 4) % INPUT_POOL_SOURCES;
         unsigned slot = (r >> 8) % 6;
         if (num[src][slot] >= 0)
         {
            input_device_pool_release(&pool, src, slot);
            num[src][slot] = -1;
         }
         else
            num[src][slot] = input_device_pool_claim(&pool, src, slot);
      }

      /* invariants */
      for (i = 0; i < INPUT_POOL_DEVICES; i++)
         if (driver_on[i])
            held |= 1u << i;
      CHECK(pool.driver == held, "driver numbers are not its pads' slots");
      for (s = 0; s < INPUT_POOL_SOURCES; s++)
         for (i = 0; i < INPUT_POOL_SLOTS; i++)
            if (num[s][i] >= 0)
            {
               CHECK(!(held & (1u << num[s][i])), "a number is held twice");
               held |= 1u << num[s][i];
               CHECK(pool.index[s][i] == num[s][i], "a pad's number changed");
            }
      CHECK(popcount16(pool.driver) + popcount16(pool.beside)
            == popcount16(held), "the pool's count is not the pads'");
      if (failures)
         break;
   }
}

int main(void)
{
   driver_alone();
   beside();
   collision();
   churn();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] device_pool_test\n");
   return 0;
}

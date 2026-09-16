/* retro_sleep_until_us: never early against the microsecond clock,
 * and overshoot in the range the limiter's margin is built to eat.
 * The late bound is generous - a loaded CI box may hold a thread -
 * but an early return is a contract violation at any load. */
#include <stdio.h>
#include <stdlib.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

int main(void)
{
   static const int64_t asks_us[] = { 300, 1000, 3000, 15000 };
   size_t i;
   int64_t worst_over = 0;

   for (i = 0; i < sizeof(asks_us) / sizeof(asks_us[0]); i++)
   {
      int lap;
      for (lap = 0; lap < 20; lap++)
      {
         int64_t start    = cpu_features_get_time_usec();
         int64_t deadline = start + asks_us[i];
         int64_t now;
         retro_sleep_until_us(deadline);
         now = cpu_features_get_time_usec();
         if (now < deadline)
         {
            fprintf(stderr, "EARLY: asked %lld us, returned %lld us short\n",
                  (long long)asks_us[i], (long long)(deadline - now));
            return 1;
         }
         if (now - deadline > worst_over)
            worst_over = now - deadline;
      }
   }
   /* 20 ms of overshoot under load is a held thread, not a broken
    * primitive; anything beyond that fails loudly. */
   if (worst_over > 20000)
   {
      fprintf(stderr, "LATE: worst overshoot %lld us\n",
            (long long)worst_over);
      return 1;
   }
   printf("retro_sleep_until_us: never early; worst overshoot %lld us\n",
         (long long)worst_over);
   return 0;
}

/* cpu_features_perf_to_usec(): durations taken on the perf counter,
 * told in microseconds by the rate cpu_features_perf_calibrate()
 * states or measures, against the clock they replace. */
#include <stdio.h>
#include <stdlib.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

static int failures;

#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); failures++; } } while (0)

/* Within 2% and 200 us of what the clock says: the counter and the
 * clock are read a few instructions apart, and a sleep's span is long */
static int within(retro_time_t got, retro_time_t want)
{
   retro_time_t d = got > want ? got - want : want - got;
   return d <= want / 50 + 200;
}

int main(void)
{
   retro_perf_tick_t p0, p1;
   retro_time_t u0, u1, got, want;

   /* As the frame loop does: once a frame, with the frame's reading */
   cpu_features_perf_calibrate(cpu_features_get_time_usec());
   retro_sleep(5);
   cpu_features_perf_calibrate(cpu_features_get_time_usec());

   p0 = cpu_features_get_perf_counter();
   u0 = cpu_features_get_time_usec();
   retro_sleep(50);
   p1 = cpu_features_get_perf_counter();
   u1 = cpu_features_get_time_usec();
   want = u1 - u0;
   got  = cpu_features_perf_to_usec(p1 - p0);
   printf("50 ms sleep: clock %lld us, counter %lld us\n",
         (long long)want, (long long)got);
   CHECK(got > 0, "the rate is known after two calibrations 5 ms apart");
   CHECK(within(got, want), "a span on the counter reads as the clock's");

   /* Ten seconds' worth of ticks, as a long stall would give: no
    * overflow in the conversion */
   if (want > 0)
   {
      retro_perf_tick_t ten = (p1 - p0) * (retro_perf_tick_t)(10000000 / want);
      retro_time_t expect   = (retro_time_t)(10000000 / want) * want;
      CHECK(within(cpu_features_perf_to_usec(ten), expect),
            "ten seconds of ticks convert without overflow");
   }

   /* A clock that steps back starts a new span and keeps the rate */
   cpu_features_perf_calibrate(u1 - 10000000);
   cpu_features_perf_calibrate(u1 - 9999000);
   CHECK(within(cpu_features_perf_to_usec(p1 - p0), want),
         "a clock stepped back leaves the rate as it was");

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("ALL OK");
   return 0;
}

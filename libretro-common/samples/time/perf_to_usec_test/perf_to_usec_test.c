/* cpu_features_perf_to_usec(): durations taken on the perf counter,
 * told in microseconds by the rate cpu_features_perf_calibrate()
 * states or measures, against the clock they replace; and the marks
 * built on it. */
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

   /* Where the rate is measured, nothing is measured until a conversion
    * asks for it: the first is 0, and the calibrations after it measure */
   cpu_features_perf_calibrate(cpu_features_get_time_usec());
   cpu_features_perf_to_usec(1000);
   /* As the runloop does: once an iteration, with the iteration's
    * reading */
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
   CHECK(got > 0, "the rate is known after a conversion asked for it and "
         "two calibrations 5 ms apart");
   CHECK(cpu_features_perf_to_usec(p0 - p1) == 0,
         "a span that runs backwards is 0, not an overflow");
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

   /* A span turned into ticks and back is the span, and a wait spun to
    * a counter reading ends at the clock's instant for it */
   {
      retro_perf_tick_t span = cpu_features_perf_from_usec(20000);
      retro_perf_tick_t t0   = cpu_features_get_perf_counter();
      retro_time_t c0        = cpu_features_get_time_usec();
      CHECK(span > 0 && within(cpu_features_perf_to_usec(span), 20000),
            "20 ms in ticks converts back to 20 ms");
      CHECK(cpu_features_perf_from_usec(0) == 0
            && cpu_features_perf_from_usec(-5) == 0,
            "no span, no ticks");
      while ((int64_t)(t0 + span - cpu_features_get_perf_counter()) > 0) { }
      CHECK(within(cpu_features_get_time_usec() - c0, 20000),
            "a spin to a counter reading 20 ms on ends 20 ms on");
   }

   /* A mark span reads as the clock's whichever a mark is here */
   {
      retro_perf_tick_t m0 = CPU_FEATURES_MARK();
      retro_time_t c0      = cpu_features_get_time_usec();
      retro_sleep(20);
      CHECK(within(CPU_FEATURES_MARK_TO_USEC(CPU_FEATURES_MARK() - m0),
               cpu_features_get_time_usec() - c0),
            "a span between two marks reads as the clock's");
   }

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("ALL OK");
   return 0;
}

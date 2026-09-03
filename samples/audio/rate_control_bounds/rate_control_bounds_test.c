/* The rate control against a driver that over-reports.
 *
 * write_avail() is sampled once per frame and compared with half of
 * buffer_size(): direction = (avail - half) / half, rate_adjust =
 * 1 + delta * direction. The contract keeps write_avail() at or below
 * buffer_size(), so direction stays within [-1, +1] and rate_adjust
 * within 1 +- delta. A driver that counts a stage in write_avail() it
 * left out of buffer_size() - shared-mode WASAPI did, before it
 * reported both - pushed direction past +1 and the adjustment with
 * it; the frontend clamps the sample to the buffer so the controller
 * cannot be driven outside its band by such a driver.
 *
 * Includes audio/audio_driver.c so the shipping computation runs,
 * with a scripted driver whose write_avail() reports whatever the
 * case sets. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>

#include "../../../audio/audio_driver.c"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static size_t scripted_avail = 0;

static size_t dev_write_avail(void *data)  { (void)data; return scripted_avail; }
static size_t dev_buffer_size(void *data)  { (void)data; return 1000; }
static void  *dev_init(const char *d, unsigned r, unsigned l, unsigned b, unsigned *n)
{ static int h; (void)d; (void)r; (void)l; (void)b; (void)n; return &h; }
static ssize_t dev_write(void *d, const void *b, size_t s) { (void)d; (void)b; return (ssize_t)s; }
static bool   dev_stop(void *d)               { (void)d; return true; }
static bool   dev_start(void *d, bool s)      { (void)d; (void)s; return true; }
static bool   dev_alive(void *d)              { (void)d; return true; }
static void   dev_nonblock(void *d, bool s)   { (void)d; (void)s; }
static void   dev_free(void *d)               { (void)d; }
static bool   dev_use_float(void *d)          { (void)d; return false; }

static audio_driver_t scripted = {
   dev_init, dev_write, dev_stop, dev_start, dev_alive, dev_nonblock,
   dev_free, dev_use_float, "scripted", NULL, NULL, dev_write_avail,
   dev_buffer_size, NULL, NULL
};

/* One DRC evaluation: enough samples since the last to cross the
 * threshold, then the computation. */
static double adjust_for(size_t avail)
{
   audio_driver_state_t *st = &audio_driver_st;
   scripted_avail           = avail;
   st->samples_since_drc    = st->drc_threshold_int16s;
   return audio_driver_compute_rate_adjust(st);
}

int main(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   double delta             = 0.005;
   double a;

   memset(st, 0, sizeof(*st));
   st->current_audio        = &scripted;
   st->context_audio_data   = scripted.init(NULL, 48000, 64, 0, NULL);
   st->buffer_size          = scripted.buffer_size(st->context_audio_data);
   st->rate_control_delta   = delta;
   st->src_ratio_orig       = 1.0;
   st->drc_threshold_int16s = 1600;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_CONTROL);

   /* At the setpoint: no adjustment. */
   a = adjust_for(500);
   CHECK(fabs(a - 1.0) < 1e-9, "at half: adjust %.6f, expected 1.0", a);

   /* Empty and full: the band's edges exactly. */
   a = adjust_for(0);
   CHECK(fabs(a - (1.0 - delta)) < 1e-9, "empty: adjust %.6f, expected %.6f", a, 1.0 - delta);
   a = adjust_for(1000);
   CHECK(fabs(a - (1.0 + delta)) < 1e-9, "full: adjust %.6f, expected %.6f", a, 1.0 + delta);

   /* Over-reporting: five times the buffer. Unclamped, direction would
    * be +9 and the adjustment 1 + 9 * delta; clamped, it is the band's
    * upper edge and no more. */
   a = adjust_for(5000);
   CHECK(a <= 1.0 + delta + 1e-9,
         "over-report: adjust %.6f exceeds the band's %.6f", a, 1.0 + delta);
   CHECK(fabs(a - (1.0 + delta)) < 1e-9,
         "over-report: adjust %.6f, expected the clamped %.6f", a, 1.0 + delta);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rate control: stays within 1 +/- delta whatever write_avail() reports\n");
   return 0;
}

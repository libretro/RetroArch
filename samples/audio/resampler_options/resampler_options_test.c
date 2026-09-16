#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/sinc_resampler.h>
#include <features/features_cpu.h>
#include <file/config_file_userdata.h>

/* Config callbacks must not be used without their required userdata. */
uint64_t cpu_features_get(void) { return 0; }
int config_userdata_get_float(void *u, const char *k, float *v, float d) { abort(); return 0; }
int config_userdata_get_int(void *u, const char *k, int *v, int d) { abort(); return 0; }
int config_userdata_get_float_array(void *u, const char *k, float **v, unsigned *n, const float *d, unsigned c) { abort(); return 0; }
int config_userdata_get_int_array(void *u, const char *k, int **v, unsigned *n, const int *d, unsigned c) { abort(); return 0; }
int config_userdata_get_string(void *u, const char *k, char **v, const char *d) { abort(); return 0; }
void config_userdata_free(void *p) { abort(); }

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)
static float input[512 * 2], actual[8192 * 2], expected[8192 * 2];

static void compare(void *a, const retro_resampler_t *ad, void *b,
      const retro_resampler_t *bd, double ratio)
{
   struct resampler_data da, db;
   unsigned step;
   for (step = 0; step < 3; step++)
   {
      da.data_in = input; da.data_out = actual; da.input_frames = 512;
      da.ratio = ratio * (step == 0 ? 1.0 : 0.9995);
      db = da; db.data_out = expected;
      ad->process(a, &da); bd->process(b, &db);
      CHECK(da.output_frames == db.output_frames);
      CHECK(memcmp(actual, expected, da.output_frames * 2 * sizeof(float)) == 0);
   }
}

static void sinc_cases(void)
{
   const double ratios[] = {0.5, 1, 1.9999, 2, 4, 8};
   const char *names[] = {"sinc", "SINC", NULL, "missing-backend"};
   unsigned r, n, h;
   void *state = NULL;
   const retro_resampler_t *driver = NULL;
   for (r = 0; r < 6; r++)
      for (n = 0; n < 4; n++)
         for (h = 0; h < 2; h++)
         {
            void *reference;
            CHECK(retro_resampler_realloc_hq(&state, &driver, names[n],
                     RESAMPLER_QUALITY_NORMAL, ratios[r], h != 0));
            CHECK(driver == &sinc_resampler);
            if (!state) exit(2);
            reference = sinc_resampler_init_hq(ratios[r], RESAMPLER_QUALITY_NORMAL, 0, h);
            if (!reference) exit(2);
            compare(state, driver, reference, &sinc_resampler, ratios[r]);
            sinc_resampler.free(reference);
         }
   /* Legacy creation must discard a previous HQ selection. */
   CHECK(retro_resampler_realloc(&state, &driver, "sinc", RESAMPLER_QUALITY_NORMAL, 4));
   {
      void *reference = sinc_resampler.init(NULL, 4, RESAMPLER_QUALITY_NORMAL, 0);
      if (!reference) exit(2);
      compare(state, driver, reference, &sinc_resampler, 4);
      sinc_resampler.free(reference);
   }
   CHECK(!retro_resampler_realloc_hq(&state, &driver, "sinc", RESAMPLER_QUALITY_NORMAL, 0, true));
   CHECK(!state && !driver);
   CHECK(retro_resampler_realloc(&state, &driver, "sinc", RESAMPLER_QUALITY_NORMAL, 4));
   /* A failing backend leaves no stale handle or dispatch pointer. */
   CHECK(!retro_resampler_realloc_hq(&state, &driver, "null", RESAMPLER_QUALITY_NORMAL, 4, true));
   CHECK(!state && !driver);
#ifdef HAVE_NEAREST_RESAMPLER
   CHECK(retro_resampler_realloc_hq(&state, &driver, "nearest", RESAMPLER_QUALITY_NORMAL, 4, true));
   CHECK(driver == &nearest_resampler);
   {
      void *reference = nearest_resampler.init(NULL, 4, RESAMPLER_QUALITY_NORMAL, 0);
      if (!reference || !state) exit(2);
      compare(state, driver, reference, &nearest_resampler, 4);
      nearest_resampler.free(reference);
   }
   driver->free(state);
#endif
}

static void independent_instances(void)
{
   void *off = NULL, *on = NULL;
   const retro_resampler_t *off_driver = NULL, *on_driver = NULL;
   void *off_reference, *on_reference;
   CHECK(retro_resampler_realloc(&off, &off_driver, "sinc", RESAMPLER_QUALITY_NORMAL, 4));
   CHECK(retro_resampler_realloc_hq(&on, &on_driver, "sinc", RESAMPLER_QUALITY_NORMAL, 4, true));
   off_reference = sinc_resampler_init_hq(4, RESAMPLER_QUALITY_NORMAL, 0, 0);
   on_reference = sinc_resampler_init_hq(4, RESAMPLER_QUALITY_NORMAL, 0, 1);
   if (!off || !on || !off_reference || !on_reference) exit(2);
   compare(off, off_driver, off_reference, &sinc_resampler, 4);
   compare(on, on_driver, on_reference, &sinc_resampler, 4);
   compare(off, off_driver, off_reference, &sinc_resampler, 4);
   sinc_resampler.free(off_reference);
   sinc_resampler.free(on_reference);
   off_driver->free(off);
   on_driver->free(on);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < 512 * 2; i++) input[i] = ((int)(i % 71) - 35) / 64.0f;
   sinc_cases();
   independent_instances();
   printf("Resampler options: %u failures\n", failures);
   return failures != 0;
}

/* Every DSP filter parameter comes from a .dsp preset the user picked, so
 * it is whatever the file says: zero, negative, enormous, or the literal
 * "inf" and "nan" that rstrtof accepts. A filter has to survive all of
 * them - no integer divide by zero, no out-of-range float-to-integer
 * conversion, no allocation sized by the preset, no loop whose trip count
 * the preset sets.
 *
 * The harness asks each filter which parameters it reads, then re-inits it
 * once per parameter per poison value and runs audio through both the float
 * and the int16 path. A new parameter is covered the moment a filter reads
 * it. Run under UBSan for the conversions and the divides; the alarm below
 * is what catches a loop that never ends. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include <libretro_dspfilter.h>

static unsigned failures;
#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("   FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; \
      } \
   } while (0)

/* ------------------------------------------------------------------ */

/* The poison set: what a preset can actually say. rstrtof parses "inf"
 * and "nan", and 1e400 overflows to infinity, so the last three are
 * reachable from a text file; they are built at run time to keep the
 * constants in range. */
static float poison[] = {
   0.0f, -0.0f, -1.0f, -1e9f, 1e9f, 1e-9f, 3.4e38f,
   0.0f, 0.0f, 0.0f  /* inf, -inf, nan */
};
#define POISON_N (sizeof(poison) / sizeof(poison[0]))

static void poison_init(void)
{
   static volatile float zero = 0.0f;
   static volatile float one  = 1.0f;
   poison[POISON_N - 3] =  one  / zero;
   poison[POISON_N - 2] = -one  / zero;
   poison[POISON_N - 1] =  zero / zero;
}

static const int poison_i[] = { 0, -1, 1, 31, 32, 33, 64, -2147483647 - 1, 2147483647 };
#define POISON_I_N (sizeof(poison_i) / sizeof(poison_i[0]))

/* One run's state: which parameter index to poison, and with what. */
static struct
{
   const char *seen[64];   /* keys, in the order the filter asks for them */
   unsigned    seen_n;
   unsigned    target;     /* index to poison; ~0u poisons nothing */
   float       value;
   int         value_i;
   int         use_int;
} cfg;

static int cfg_note(const char *key)
{
   unsigned i;
   for (i = 0; i < cfg.seen_n; i++)
      if (!strcmp(cfg.seen[i], key))
         return (int)i;
   if (cfg.seen_n < sizeof(cfg.seen) / sizeof(cfg.seen[0]))
      cfg.seen[cfg.seen_n++] = key;
   return (int)cfg.seen_n - 1;
}

static int cfg_get_float(void *u, const char *key, float *value, float def)
{
   int idx = cfg_note(key);
   (void)u;
   if ((unsigned)idx == cfg.target && !cfg.use_int)
   {
      *value = cfg.value;
      return 1;
   }
   *value = def;
   return 0;
}

static int cfg_get_int(void *u, const char *key, int *value, int def)
{
   int idx = cfg_note(key);
   (void)u;
   if ((unsigned)idx == cfg.target && cfg.use_int)
   {
      *value = cfg.value_i;
      return 1;
   }
   *value = def;
   return 0;
}

static int cfg_get_float_array(void *u, const char *key, float **values,
      unsigned *out_num_values, const float *def, unsigned num_def)
{
   int      idx = cfg_note(key);
   unsigned i;
   float   *v;
   (void)u;
   if (!(v = (float*)malloc(num_def * sizeof(float) + sizeof(float))))
   {
      *values = NULL;
      *out_num_values = 0;
      return 0;
   }
   for (i = 0; i < num_def; i++)
      v[i] = def[i];
   *out_num_values = num_def;
   *values         = v;
   if ((unsigned)idx == cfg.target && !cfg.use_int)
   {
      for (i = 0; i < num_def; i++)
         v[i] = cfg.value;
      return 1;
   }
   return 0;
}

static int cfg_get_int_array(void *u, const char *key, int **values,
      unsigned *out_num_values, const int *def, unsigned num_def)
{
   int      idx = cfg_note(key);
   unsigned i;
   int     *v;
   (void)u;
   if (!(v = (int*)malloc(num_def * sizeof(int) + sizeof(int))))
   {
      *values = NULL;
      *out_num_values = 0;
      return 0;
   }
   for (i = 0; i < num_def; i++)
      v[i] = def[i];
   *out_num_values = num_def;
   *values         = v;
   if ((unsigned)idx == cfg.target && cfg.use_int)
   {
      for (i = 0; i < num_def; i++)
         v[i] = cfg.value_i;
      return 1;
   }
   return 0;
}

static int cfg_get_string(void *u, const char *key, char **output,
      const char *def)
{
   (void)u; (void)key;
   *output = strdup(def);
   return 0;
}

static void cfg_free(void *ptr) { free(ptr); }

static const struct dspfilter_config config = {
   cfg_get_float, cfg_get_int,
   cfg_get_float_array, cfg_get_int_array,
   cfg_get_string, cfg_free
};

/* ------------------------------------------------------------------ */

/* A filter that never returns has to be reported as a failure rather than
 * hanging the suite. */
static const char *stuck_filter;
static const char *stuck_key;
static float       stuck_value;

static void on_alarm(int sig)
{
   (void)sig;
   printf("   FAIL: %s did not return with %s = %g "
          "(a loop the preset sizes)\n",
         stuck_filter ? stuck_filter : "?",
         stuck_key ? stuck_key : "?", stuck_value);
   fflush(stdout);
   _exit(2);
}

#define FRAMES 512

static void run_once(const struct dspfilter_implementation *impl,
      unsigned rate)
{
   static float   fbuf[FRAMES * 2];
   static int16_t ibuf[FRAMES * 2];
   struct dspfilter_info info;
   struct dspfilter_output      out;
   struct dspfilter_input       in;
   struct dspfilter_output_i16  out_i;
   struct dspfilter_input_i16   in_i;
   unsigned i, k;
   void *h;

   info.input_rate = rate;
   if (!(h = impl->init(&info, &config, NULL)))
      return;   /* refusing the preset is a correct outcome */

   for (i = 0; i < FRAMES * 2; i++)
   {
      fbuf[i] = (i & 1) ? 0.5f : -0.5f;
      ibuf[i] = (int16_t)((i & 1) ? 16384 : -16384);
   }
   /* Several blocks, so a block filter reaches its own buffering. */
   for (k = 0; k < 8; k++)
   {
      in.samples  = fbuf;
      in.frames   = FRAMES;
      out.samples = NULL;
      out.frames  = 0;
      impl->process(h, &out, &in);

      if (impl->api_version >= 2 && impl->process_i16)
      {
         in_i.samples  = ibuf;
         in_i.frames   = FRAMES;
         out_i.samples = NULL;
         out_i.frames  = 0;
         impl->process_i16(h, &out_i, &in_i);
      }
   }
   impl->free(h);
}

static void sweep(const struct dspfilter_implementation *impl)
{
   /* 0 is what info->input_rate holds when the audio device never
    * opened, and the low rates are where a table sized in seconds
    * rounds down to nothing. */
   static const unsigned rates[] = { 48000, 8000, 100, 1, 0 };
   unsigned p, v, r, params;

   printf("   %s\n", impl->ident);
   stuck_filter = impl->ident;

   /* Discovery: default everything, and learn the parameter names. */
   cfg.seen_n = 0;
   cfg.target = (unsigned)~0u;
   cfg.use_int = 0;
   stuck_key   = "(defaults)";
   stuck_value = 0.0f;
   alarm(20);
   run_once(impl, 48000);
   alarm(0);
   params = cfg.seen_n;

   for (p = 0; p < params; p++)
   {
      stuck_key = cfg.seen[p];
      for (r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
      {
         for (v = 0; v < POISON_N; v++)
         {
            cfg.target  = p;
            cfg.use_int = 0;
            cfg.value   = poison[v];
            stuck_value = poison[v];
            alarm(20);
            run_once(impl, rates[r]);
            alarm(0);
         }
         for (v = 0; v < POISON_I_N; v++)
         {
            cfg.target  = p;
            cfg.use_int = 1;
            cfg.value_i = poison_i[v];
            stuck_value = (float)poison_i[v];
            alarm(20);
            run_once(impl, rates[r]);
            alarm(0);
         }
      }
   }
   CHECK(params > 0, "%s read no parameters", impl->ident);
}

/* Each filter is swept in its own process, so one that dies on a preset
 * still leaves the other fourteen swept and named in the report. */
static void sweep_forked(const struct dspfilter_implementation *impl)
{
   pid_t pid;
   int   status = 0;

   fflush(NULL);
   if ((pid = fork()) < 0)
   {
      sweep(impl);
      return;
   }
   if (pid == 0)
   {
      failures = 0;
      sweep(impl);
      fflush(NULL);
      _exit(failures ? 1 : 0);
   }
   while (waitpid(pid, &status, 0) < 0)
      ;
   if (WIFSIGNALED(status))
      CHECK(0, "%s died on a preset (signal %d)", impl->ident,
            WTERMSIG(status));
   else if (WEXITSTATUS(status))
      failures++;
}

/* ------------------------------------------------------------------ */

#define FILTER(sym) \
   extern const struct dspfilter_implementation * \
      sym##_dspfilter_get_implementation(dspfilter_simd_mask_t)

FILTER(bitcrusher);
FILTER(chorus);
FILTER(delta);          /* crystalizer */
FILTER(echo);
FILTER(eq);
FILTER(iir);
FILTER(overdrive);
FILTER(panning);
FILTER(phaser);
FILTER(reverb);
FILTER(earlyreverb);
FILTER(tremolo);
FILTER(vibrato);
FILTER(wahwah);
FILTER(wsolapitchtempo);

int main(void)
{
   const struct dspfilter_implementation *(*const entries[])(
         dspfilter_simd_mask_t) = {
      bitcrusher_dspfilter_get_implementation,
      chorus_dspfilter_get_implementation,
      delta_dspfilter_get_implementation,
      echo_dspfilter_get_implementation,
      eq_dspfilter_get_implementation,
      iir_dspfilter_get_implementation,
      overdrive_dspfilter_get_implementation,
      panning_dspfilter_get_implementation,
      phaser_dspfilter_get_implementation,
      reverb_dspfilter_get_implementation,
      earlyreverb_dspfilter_get_implementation,
      tremolo_dspfilter_get_implementation,
      vibrato_dspfilter_get_implementation,
      wahwah_dspfilter_get_implementation,
      wsolapitchtempo_dspfilter_get_implementation
   };
   unsigned i;

   setbuf(stdout, NULL);
   poison_init();
   signal(SIGALRM, on_alarm);
   printf("dsp filter config:\n");
   for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
   {
      const struct dspfilter_implementation *impl = entries[i](0);
      if (!impl)
      {
         CHECK(0, "entry %u returned no implementation", i);
         continue;
      }
      sweep_forked(impl);
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("dsp filter config: every filter survives every parameter set to "
          "zero, negative, huge, infinity and NaN\n");
   return 0;
}

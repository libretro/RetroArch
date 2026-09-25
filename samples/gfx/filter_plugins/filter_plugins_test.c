/* Loads every video filter and audio DSP filter plugin given on the
 * command line and runs data through each one.
 *
 * Each plugin is opened with RTLD_NOW, so a symbol it uses but cannot
 * resolve - a math function from a libm it was not linked against -
 * fails the load here instead of depending on the host process
 * happening to have that library loaded already.  This program calls
 * no math functions itself and is not linked against libm, so it
 * cannot paper over one.
 *
 * Video filters (.so under video_filters) take one frame in each input
 * format they support, and those that take more than one worker must
 * give the single-worker output byte for byte with 2, 3 and 4 workers
 * running at once; audio filters (under dsp_filters) take a block
 * through the float path and, when the plugin has one, the int16 path.
 * Every setting is left at the default the plugin asks for.  Returns
 * 0 when every plugin loads and runs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <pthread.h>

#include "softfilter.h"
#include <libretro_dspfilter.h>

static unsigned failures;

#define FAIL(...) do { printf("FAIL  " __VA_ARGS__); printf("\n"); failures++; } while (0)

/* Config getters: every key takes the default the plugin passes. */
static int c_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; *v = d; return 0; }
static int c_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; *v = d; return 0; }
static int c_hex(void *u, const char *k, unsigned *v, unsigned d)
{ (void)u; (void)k; *v = d; return 0; }
static int c_float_array(void *u, const char *k, float **v, unsigned *n,
      const float *d, unsigned nd)
{
   (void)u; (void)k;
   *v = (float*)malloc((nd ? nd : 1) * sizeof(float));
   if (nd && *v)
      memcpy(*v, d, nd * sizeof(float));
   *n = *v ? nd : 0;
   return 0;
}
static int c_int_array(void *u, const char *k, int **v, unsigned *n,
      const int *d, unsigned nd)
{
   (void)u; (void)k;
   *v = (int*)malloc((nd ? nd : 1) * sizeof(int));
   if (nd && *v)
      memcpy(*v, d, nd * sizeof(int));
   *n = *v ? nd : 0;
   return 0;
}
static int c_string(void *u, const char *k, char **o, const char *d)
{
   size_t len = d ? strlen(d) : 0;
   (void)u; (void)k;
   if ((*o = (char*)malloc(len + 1)))
      memcpy(*o, d ? d : "", len + 1);
   return 0;
}

#define IN_W 320
#define IN_H 240

/* Worker-count check: an odd size, so no slicing divides it evenly,
 * and a few frames, so filters that carry state between frames are
 * compared on it too. */
#define SPLIT_W      317
#define SPLIT_H      223
#define SPLIT_FRAMES 3

static void *run_packet(void *arg)
{
   struct softfilter_work_packet **pp = (struct softfilter_work_packet**)arg;
   (*pp)->work(pp[1] ? (void*)pp[1] : NULL, (*pp)->thread_data);
   return NULL;
}

/* One frame the way RetroArch's dispatcher runs it: every packet at
 * once, each on its own thread. */
static void run_frame_threaded(const struct softfilter_implementation *impl,
      void *filt, struct softfilter_work_packet *packets, unsigned threads,
      void *out, size_t out_stride, const void *in, unsigned w, unsigned h,
      size_t in_stride)
{
   unsigned t;
   pthread_t tid[16];
   struct softfilter_work_packet *arg[16][2];
   impl->get_work_packets(filt, packets, out, out_stride, in, w, h, in_stride);
   for (t = 0; t < threads && t < 16; t++)
   {
      arg[t][0] = &packets[t];
      arg[t][1] = (struct softfilter_work_packet*)filt;
      if (!packets[t].work || pthread_create(&tid[t], NULL, run_packet, arg[t]))
         tid[t] = 0;
   }
   for (t = 0; t < threads && t < 16; t++)
      if (tid[t])
         pthread_join(tid[t], NULL);
}

/* Runs SPLIT_FRAMES frames with the given worker count; returns the
 * last frame's output, or NULL when the filter does not take that
 * many workers (it reports fewer), which is not a failure. */
static uint8_t *run_split(const struct softfilter_implementation *impl,
      const struct softfilter_config *cfg, unsigned in_fmt, unsigned out_fmt,
      unsigned threads, const uint8_t *in, size_t in_stride,
      size_t *out_size, unsigned *used)
{
   unsigned ow = 0, oh = 0, f, n;
   size_t out_bpp = out_fmt == SOFTFILTER_FMT_RGB565 ? 2 : 4, out_stride;
   uint8_t *out;
   struct softfilter_work_packet packets[16];
   void *filt = impl->create(cfg, in_fmt, out_fmt, SPLIT_W, SPLIT_H,
         threads, 0, NULL);
   if (!filt)
      return NULL;
   n = impl->query_num_threads(filt);
   *used = n;
   if (n > 16 || (threads > 1 && n < 2))
   {
      impl->destroy(filt);
      return NULL;
   }
   impl->query_output_size(filt, &ow, &oh, SPLIT_W, SPLIT_H);
   out_stride = ow * out_bpp + 32;
   *out_size  = out_stride * oh;
   if (!(out = (uint8_t*)malloc(*out_size)))
   {
      impl->destroy(filt);
      return NULL;
   }
   memset(out, 0xa5, *out_size);
   memset(packets, 0, sizeof(packets));
   for (f = 0; f < SPLIT_FRAMES; f++)
      run_frame_threaded(impl, filt, packets, n, out, out_stride,
            in, SPLIT_W, SPLIT_H, in_stride);
   impl->destroy(filt);
   return out;
}

/* Every worker count the filter takes must give the single-worker
 * output, byte for byte. */
static void check_split(const char *path,
      const struct softfilter_implementation *impl,
      const struct softfilter_config *cfg, unsigned in_fmt, unsigned out_fmt)
{
   size_t in_bpp    = in_fmt == SOFTFILTER_FMT_RGB565 ? 2 : 4;
   size_t in_stride = SPLIT_W * in_bpp + 48;
   size_t size1 = 0, sizen = 0, i;
   unsigned threads, used = 0, used1 = 0;
   uint8_t *in = (uint8_t*)malloc(in_stride * SPLIT_H), *ref;
   if (!in)
      return;
   for (i = 0; i < in_stride * SPLIT_H; i++)
      in[i] = (uint8_t)(((i * 2654435761u) >> 24) & ((i / in_stride) % 7 ? 0xff : 0xf0));
   if (!(ref = run_split(impl, cfg, in_fmt, out_fmt, 1, in, in_stride, &size1, &used1)))
   {
      free(in);
      return;
   }
   for (threads = 2; threads <= 4; threads++)
   {
      uint8_t *got = run_split(impl, cfg, in_fmt, out_fmt, threads, in,
            in_stride, &sizen, &used);
      if (!got)
         continue;
      if (sizen != size1 || memcmp(ref, got, size1))
      {
         size_t first = 0;
         while (first < size1 && ref[first] == got[first])
            first++;
         FAIL("%s: format %u with %u workers differs from one worker (first at byte %lu of %lu)",
               path, in_fmt, used, (unsigned long)first, (unsigned long)size1);
      }
      free(got);
   }
   free(ref);
   free(in);
}

static void test_video(const char *path, void *lib)
{
   static const unsigned fmts[2] = { SOFTFILTER_FMT_RGB565, SOFTFILTER_FMT_XRGB8888 };
   struct softfilter_config cfg;
   const struct softfilter_implementation *impl;
   softfilter_get_implementation_t get = (softfilter_get_implementation_t)
      dlsym(lib, "softfilter_get_implementation");
   unsigned f, ran = 0;
   unsigned before = failures;

   cfg.get_float       = c_float;
   cfg.get_int         = c_int;
   cfg.get_hex         = c_hex;
   cfg.get_float_array = c_float_array;
   cfg.get_int_array   = c_int_array;
   cfg.get_string      = c_string;
   cfg.free            = free;

   if (!get || !(impl = get(0)))
   {
      FAIL("%s: no softfilter_get_implementation", path);
      return;
   }
   if (impl->api_version != SOFTFILTER_API_VERSION)
   {
      FAIL("%s: api_version %u, expected %u", path, impl->api_version,
            SOFTFILTER_API_VERSION);
      return;
   }

   for (f = 0; f < 2; f++)
   {
      unsigned in_fmt  = fmts[f];
      unsigned outs, out_fmt, ow = 0, oh = 0, threads, t, y;
      size_t in_bpp, out_bpp, in_stride, out_stride;
      uint8_t *in, *out;
      struct softfilter_work_packet *packets;
      void *filt;

      if (!(impl->query_input_formats() & in_fmt))
         continue;
      outs    = impl->query_output_formats(in_fmt);
      out_fmt = (outs & in_fmt) ? in_fmt
              : (outs & SOFTFILTER_FMT_XRGB8888) ? SOFTFILTER_FMT_XRGB8888
              : SOFTFILTER_FMT_RGB565;
      if (!(outs & out_fmt))
      {
         FAIL("%s: input format %u has no usable output format", path, in_fmt);
         continue;
      }
      if (!(filt = impl->create(&cfg, in_fmt, out_fmt, IN_W, IN_H, 2, 0, NULL)))
      {
         FAIL("%s: create failed for input format %u", path, in_fmt);
         continue;
      }

      impl->query_output_size(filt, &ow, &oh, IN_W, IN_H);
      if (!ow || !oh || ow > IN_W * 8 || oh > IN_H * 8)
      {
         FAIL("%s: output size %ux%u for a %ux%u input", path, ow, oh, IN_W, IN_H);
         impl->destroy(filt);
         continue;
      }

      in_bpp     = in_fmt  == SOFTFILTER_FMT_RGB565 ? 2 : 4;
      out_bpp    = out_fmt == SOFTFILTER_FMT_RGB565 ? 2 : 4;
      /* Padded rows on both sides, as frontends hand them over. */
      in_stride  = IN_W * in_bpp + 64;
      out_stride = ow * out_bpp + 64;
      in         = (uint8_t*)malloc(in_stride * IN_H);
      out        = (uint8_t*)calloc(out_stride, oh);
      threads    = impl->query_num_threads(filt);
      packets    = (struct softfilter_work_packet*)
         calloc(threads ? threads : 1, sizeof(*packets));

      if (!in || !out || !packets || !threads)
      {
         FAIL("%s: %s", path, threads ? "out of memory" : "reports zero threads");
         free(in); free(out); free(packets);
         impl->destroy(filt);
         continue;
      }
      for (y = 0; y < in_stride * IN_H; y++)
         in[y] = (uint8_t)(y * 2654435761u >> 24);

      impl->get_work_packets(filt, packets, out, out_stride,
            in, IN_W, IN_H, in_stride);
      for (t = 0; t < threads; t++)
         if (packets[t].work)
            packets[t].work(filt, packets[t].thread_data);

      impl->destroy(filt);
      free(in);
      free(out);
      free(packets);
      check_split(path, impl, &cfg, in_fmt, out_fmt);
      ran++;
   }

   if (!ran && failures == before)
      FAIL("%s: supports neither RGB565 nor XRGB8888 input", path);
   else if (ran && failures == before)
      printf("ok    %s (%s)\n", path, impl->short_ident ? impl->short_ident : "?");
}

#define AUDIO_FRAMES 2048

/* A triangle wave with a different period per channel, continuing
 * from block to block.  Refilled before every call: a plugin may
 * process in place and hand the input buffer back as its output, as
 * the API allows, so the previous block's output is no input. */
static void fill_audio(float *fin, int16_t *iin, unsigned block)
{
   unsigned i;
   for (i = 0; i < AUDIO_FRAMES; i++)
   {
      unsigned n = block * AUDIO_FRAMES + i;
      int l      = (int)(n % 97)  - 48;
      int r      = (int)(n % 131) - 65;
      fin[i * 2 + 0] = (float)l / 64.0f;
      fin[i * 2 + 1] = (float)r / 80.0f;
      iin[i * 2 + 0] = (int16_t)(l * 600);
      iin[i * 2 + 1] = (int16_t)(r * 450);
   }
}

/* Finite and within a generous bound, without math.h: NaN fails the
 * self-comparison and infinities fail the range check. */
static int sample_ok(float s)
{
   return s == s && s > -64.0f && s < 64.0f;
}

static void test_audio(const char *path, void *lib)
{
   struct dspfilter_config cfg;
   struct dspfilter_info info;
   const struct dspfilter_implementation *impl;
   dspfilter_get_implementation_t get = (dspfilter_get_implementation_t)
      dlsym(lib, "dspfilter_get_implementation");
   float   *fin;
   int16_t *iin;
   void *h;
   unsigned i, block;
   unsigned before = failures;

   cfg.get_float       = c_float;
   cfg.get_int         = c_int;
   cfg.get_float_array = c_float_array;
   cfg.get_int_array   = c_int_array;
   cfg.get_string      = c_string;
   cfg.free            = free;
   info.input_rate     = 48000.0f;

   if (!get || !(impl = get(0)))
   {
      FAIL("%s: no dspfilter_get_implementation", path);
      return;
   }
   if (impl->api_version != DSPFILTER_API_VERSION)
   {
      FAIL("%s: api_version %u, expected %u", path, impl->api_version,
            DSPFILTER_API_VERSION);
      return;
   }

   fin = (float*)malloc(AUDIO_FRAMES * 2 * sizeof(float));
   iin = (int16_t*)malloc(AUDIO_FRAMES * 2 * sizeof(int16_t));
   if (!fin || !iin)
   {
      FAIL("%s: out of memory", path);
      free(fin); free(iin);
      return;
   }
   if (!(h = impl->init(&info, &cfg, NULL)))
   {
      FAIL("%s: init failed", path);
      free(fin); free(iin);
      return;
   }

   /* Several blocks, so stateful filters (delay lines, overlap-add)
    * run past their first buffer. */
   for (block = 0; block < 4; block++)
   {
      struct dspfilter_input  in;
      struct dspfilter_output out;
      fill_audio(fin, iin, block);
      in.samples  = fin;
      in.frames   = AUDIO_FRAMES;
      out.samples = NULL;
      out.frames  = 0;
      impl->process(h, &out, &in);
      if (out.frames && !out.samples)
      {
         FAIL("%s: float path returned %u frames and no buffer", path, out.frames);
         break;
      }
      for (i = 0; i < out.frames * 2; i++)
         if (!sample_ok(out.samples[i]))
         {
            FAIL("%s: float path sample %u of block %u is %g", path, i, block,
                  (double)out.samples[i]);
            break;
         }
   }

   if (impl->process_i16)
   {
      for (block = 0; block < 4; block++)
      {
         struct dspfilter_input_i16  in;
         struct dspfilter_output_i16 out;
         fill_audio(fin, iin, block);
         in.samples  = iin;
         in.frames   = AUDIO_FRAMES;
         out.samples = NULL;
         out.frames  = 0;
         impl->process_i16(h, &out, &in);
         if (out.frames && !out.samples)
         {
            FAIL("%s: int16 path returned %u frames and no buffer", path, out.frames);
            break;
         }
      }
   }

   impl->free(h);
   free(fin);
   free(iin);
   if (failures == before)
      printf("ok    %s (%s%s)\n", path, impl->short_ident ? impl->short_ident : "?",
            impl->process_i16 ? ", int16" : "");
}

int main(int argc, char **argv)
{
   int i;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s plugin.so...\n", argv[0]);
      return 2;
   }

   for (i = 1; i < argc; i++)
   {
      void *lib = dlopen(argv[i], RTLD_NOW | RTLD_LOCAL);
      if (!lib)
      {
         FAIL("%s: %s", argv[i], dlerror());
         continue;
      }
      if (dlsym(lib, "softfilter_get_implementation"))
         test_video(argv[i], lib);
      else if (dlsym(lib, "dspfilter_get_implementation"))
         test_audio(argv[i], lib);
      else
         FAIL("%s: exports neither filter entry point", argv[i]);
      dlclose(lib);
   }

   printf("%d plugin(s), %u failure(s)\n", argc - 1, failures);
   return failures ? 1 : 0;
}

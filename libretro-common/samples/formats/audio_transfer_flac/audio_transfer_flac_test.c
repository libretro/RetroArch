/* FLAC through audio_transfer, sample-exact against the PCM it was
 * encoded from, in every input mode the arm has:
 *
 *   - a native .flac buffer;
 *   - FLAC in Matroska, as a buffer the arm demuxes itself, whole and
 *     windowed - resident a little at a time, raised only when a read
 *     has stalled at the wall, as a feeder runs;
 *   - demuxed input (set_demuxed_ptr): the fLaC header as setup and
 *     the frames as delimited packets, all given up front;
 *   - demuxed input grown a packet at a time, each growth only after
 *     the decoder has run dry - the progressive source the growth
 *     contract in <formats/audio.h> is written for.
 *
 * FLAC is lossless, so every lane must give back exactly the PCM that
 * went in: every sample, and not one frame more or less. The fixture
 * length is not a multiple of the encoder's block size, so the stream
 * ends on a short frame - the one a decoder holding back an incomplete
 * frame for more input would lose.
 *
 * The source is regenerated here from the same formula mkfixtures.py
 * encoded, so the fixtures carry no reference copy of their own. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <formats/audio.h>
#include <formats/rwebm.h>
#include <streams/file_stream.h>

#define SRC_FRAMES   20000
#define SRC_RATE     44100
#define SRC_CHANNELS 2

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t *src_pcm;
static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* mkfixtures.py, sample for sample: the same truncation, the same
 * low-order noise. */
static void make_source(void)
{
   unsigned i;
   src_pcm = (int16_t*)malloc(SRC_FRAMES * SRC_CHANNELS * sizeof(int16_t));
   for (i = 0; i < SRC_FRAMES; i++)
   {
      double   t  = (double)i * 2.0 * M_PI / SRC_RATE;
      int      nl = (int)((((uint64_t)i * 1103515245u + 12345u)
               & 0xffffffffu) >> 16 & 63) - 32;
      int      nr = (int)((((uint64_t)i * 69069u + 1u)
               & 0xffffffffu) >> 16 & 31) - 16;
      src_pcm[i * 2]     = (int16_t)((int)(12000.0 * sin(t * 440.0)) + nl);
      src_pcm[i * 2 + 1] = (int16_t)((int)( 9000.0 * sin(t * 660.0)) + nr);
   }
}

static void *load(const char *path, size_t *len)
{
   void   *buf = NULL;
   int64_t n   = 0;
   if (!filestream_read_file(path, &buf, &n) || n <= 0)
   {
      fprintf(stderr, "cannot read %s (run from the sample directory)\n",
            path);
      exit(2);
   }
   *len = (size_t)n;
   return buf;
}

/* Decodes everything a context has into @out, at most @cap frames,
 * in reads of @chunk. Returns the frame count; stops at END. */
static size_t drain(void *ctx, int16_t *out, size_t at, size_t cap,
      size_t chunk)
{
   for (;;)
   {
      size_t got = 0;
      size_t want = chunk;
      int    r;
      if (at + want > cap)
         want = cap - at;
      if (!want)
         return at;
      r = audio_transfer_read_s16(ctx, AUDIO_TYPE_FLAC, out + at * 2, want,
            &got);
      at += got;
      if (r == AUDIO_PROCESS_ERROR || (r == AUDIO_PROCESS_END && !got))
         return at;
   }
}

static void compare(const char *lane, const int16_t *pcm, size_t frames)
{
   size_t i;
   CHECK(frames == SRC_FRAMES, "%s: %lu frames decoded, the source has %d",
         lane, (unsigned long)frames, SRC_FRAMES);
   for (i = 0; i < frames && i < SRC_FRAMES; i++)
      if (     pcm[i * 2]     != src_pcm[i * 2]
            || pcm[i * 2 + 1] != src_pcm[i * 2 + 1])
      {
         CHECK(0, "%s: frame %lu differs from the source (%d,%d vs %d,%d)",
               lane, (unsigned long)i, pcm[i * 2], pcm[i * 2 + 1],
               src_pcm[i * 2], src_pcm[i * 2 + 1]);
         break;
      }
}

static void lane_buffer(const char *lane, const char *path)
{
   size_t   len, got;
   void    *buf = load(path, &len);
   void    *ctx = audio_transfer_new(AUDIO_TYPE_FLAC);
   int16_t *pcm = (int16_t*)calloc((SRC_FRAMES + 8192) * 2, sizeof(int16_t));
   unsigned ch = 0, rate = 0;

   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_FLAC, buf, len);
   CHECK(audio_transfer_start(ctx, AUDIO_TYPE_FLAC), "%s: start failed", lane);
   audio_transfer_info(ctx, AUDIO_TYPE_FLAC, &ch, &rate, NULL);
   CHECK(ch == SRC_CHANNELS && rate == SRC_RATE, "%s: %u ch at %u Hz",
         lane, ch, rate);
   got = drain(ctx, pcm, 0, SRC_FRAMES + 8192, 1000);
   compare(lane, pcm, got);
   audio_transfer_free(ctx, AUDIO_TYPE_FLAC);
   free(pcm);
   free(buf);
}

/* A windowed Matroska buffer: the decoder sees the file resident up to
 * @avail, raised by @step each time a read stalls at that wall. A stall
 * is NEXT with nothing produced; END while bytes are still to arrive is
 * the stream ending early, which a looping voice answers by rewinding. */
static void lane_windowed(const char *path, size_t avail, size_t step)
{
   const char *lane = "Matroska buffer, windowed";
   size_t   len, got = 0;
   void    *buf     = load(path, &len);
   void    *ctx     = audio_transfer_new(AUDIO_TYPE_FLAC);
   int16_t *pcm     = (int16_t*)calloc((SRC_FRAMES + 8192) * 2,
         sizeof(int16_t));
   unsigned stalls  = 0;

   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_FLAC, buf, len);
   audio_transfer_set_avail(ctx, AUDIO_TYPE_FLAC, avail);
   CHECK(audio_transfer_start(ctx, AUDIO_TYPE_FLAC), "%s: start failed", lane);
   for (;;)
   {
      size_t f = 0;
      int    r = audio_transfer_read_s16(ctx, AUDIO_TYPE_FLAC,
            pcm + got * 2, 1000, &f);
      got += f;
      if (r == AUDIO_PROCESS_ERROR || got >= SRC_FRAMES + 8192)
         break;
      if (f)
         continue;
      if (avail >= len)
         break;
      CHECK(r == AUDIO_PROCESS_NEXT,
            "%s: END at frame %lu with %lu of %lu bytes resident",
            lane, (unsigned long)got, (unsigned long)avail,
            (unsigned long)len);
      if (r != AUDIO_PROCESS_NEXT)
         break;
      stalls++;
      avail = (avail + step < len) ? avail + step : len;
      audio_transfer_set_avail(ctx, AUDIO_TYPE_FLAC, avail);
   }
   compare(lane, pcm, got);
   CHECK(stalls > 0, "%s: never stalled; the window did not bind", lane);
   audio_transfer_free(ctx, AUDIO_TYPE_FLAC);
   free(pcm);
   free(buf);
}

/* The Matroska fixture taken apart as a container demuxer would: the
 * CodecPrivate (a whole fLaC header) and every block of the track, laid
 * out end to end with their sizes. */
struct demuxed
{
   const uint8_t *setup;
   size_t         setup_size;
   uint8_t       *blob;
   size_t         blob_size;
   uint32_t      *sizes;
   size_t         count;
};

static void demux_mka(const uint8_t *mka, size_t len, struct demuxed *d)
{
   rwebm_t           *w = rwebm_open_memory(mka, len);
   const rwebm_track *t;
   rwebm_packet       pkt;
   size_t             cap = 0;

   memset(d, 0, sizeof(*d));
   if (!w || !(t = rwebm_get_track(w, 0)))
   {
      fprintf(stderr, "fixtures/tone.mka: no track\n");
      exit(2);
   }
   d->setup      = t->codec_private;
   d->setup_size = t->codec_private_size;
   d->blob       = (uint8_t*)malloc(len);
   while (rwebm_read_packet(w, &pkt) == 1)
   {
      if (d->count == cap)
      {
         cap      = cap ? cap * 2 : 64;
         d->sizes = (uint32_t*)realloc(d->sizes, cap * sizeof(uint32_t));
      }
      memcpy(d->blob + d->blob_size, pkt.data, pkt.size);
      d->blob_size          += pkt.size;
      d->sizes[d->count++]   = (uint32_t)pkt.size;
   }
   rwebm_close(w);
}

static void lane_demuxed(const struct demuxed *d, int grow)
{
   const char *lane = grow ? "demuxed, grown on starvation"
                           : "demuxed, every packet up front";
   void    *ctx   = audio_transfer_new(AUDIO_TYPE_FLAC);
   int16_t *pcm   = (int16_t*)calloc((SRC_FRAMES + 8192) * 2, sizeof(int16_t));
   size_t   given = grow ? 1 : d->count;
   size_t   bytes = 0, i, got = 0;
   unsigned stalls = 0;

   for (i = 0; i < given; i++)
      bytes += d->sizes[i];
   CHECK(audio_transfer_set_demuxed_ptr(ctx, AUDIO_TYPE_FLAC,
         d->setup, d->setup_size, d->blob, bytes, d->sizes, given),
         "%s: no demuxed path", lane);
   CHECK(audio_transfer_start(ctx, AUDIO_TYPE_FLAC), "%s: start failed", lane);

   for (;;)
   {
      got = drain(ctx, pcm, got, SRC_FRAMES + 8192, 1000);
      if (given == d->count)
         break;
      /* Dry: the next packet arrives, as from a progressive source. */
      stalls++;
      bytes += d->sizes[given++];
      audio_transfer_set_demuxed_ptr(ctx, AUDIO_TYPE_FLAC,
            d->setup, d->setup_size, d->blob, bytes, d->sizes, given);
   }
   /* Anything held for want of input is released by the read that
    * finds no more coming. */
   got = drain(ctx, pcm, got, SRC_FRAMES + 8192, 1000);
   compare(lane, pcm, got);
   if (grow)
      CHECK(stalls + 1 == d->count, "%s: %u stalls for %lu packets",
            lane, stalls, (unsigned long)d->count);
   audio_transfer_free(ctx, AUDIO_TYPE_FLAC);
   free(pcm);
}

int main(void)
{
   struct demuxed d;
   size_t         mka_len;
   void          *mka;

   make_source();
   lane_buffer("native .flac buffer", "fixtures/tone.flac");
   lane_buffer("Matroska buffer", "fixtures/tone.mka");
   lane_windowed("fixtures/tone.mka", 4096, 2048);

   mka = load("fixtures/tone.mka", &mka_len);
   demux_mka((const uint8_t*)mka, mka_len, &d);
   CHECK(d.count > 4, "fixtures/tone.mka: %lu packets", (unsigned long)d.count);
   lane_demuxed(&d, 0);
   lane_demuxed(&d, 1);
   free(d.blob);
   free(d.sizes);
   free(mka);
   free(src_pcm);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] audio_transfer_flac_test\n");
   return 0;
}

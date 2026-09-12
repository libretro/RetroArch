/* Checks the CoreAudio ring keeps a frame's channels together, off a
 * Mac, for every layout the driver supports.
 *
 * This started as an attempt to prove a bug I had written down and it
 * disproved it instead, so what it establishes is worth stating
 * exactly.
 *
 * The capacity is the sample count rounded up to a power of two, which
 * for six channels is not a whole number of frames - 4096 samples is
 * 682 frames and four over - so the cursors wrap four samples into a
 * frame. I had that noted as channel rotation for 5.1. It is not. The
 * ring is a FIFO: a read returns samples in the order they were
 * written whatever position they sit at, so where the wrap falls does
 * not reach the device at all. Case 3 runs the traffic and case 4
 * splits every transfer across the wrap on purpose, and the channels
 * come back intact for 6 channels exactly as they do for 2.
 *
 * What does rotate the channels is a transfer whose COUNT is not a
 * whole number of frames. That leaves the read cursor a sample behind
 * the write cursor and every frame after it is skewed, permanently.
 * Case 5 does that deliberately and fails if it is not detected, so
 * the alignment the driver applies to short reads has a guard. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The ring's arithmetic, as audio/drivers/coreaudio.c has it. */
static size_t ring_capacity(size_t frames_wanted, unsigned channels)
{
   size_t samples = frames_wanted * (size_t)channels;
   size_t cap     = 1;
   while (cap < samples)
      cap <<= 1;
   return cap;
}
#define coreaudio_ring_advance(ptr, count, cap) (((ptr) + (count)) & ((cap) - 1))

static int failures;

static void fail(const char *what, const char *detail)
{
   printf("   FAIL %s: %s\n", what, detail);
   failures++;
}

/* The ring, with only what the arithmetic needs. */
typedef struct
{
   float *buffer;
   size_t capacity;
   size_t write_ptr;
   size_t read_ptr;
   size_t filled;
} ring_t;

static void ring_write(ring_t *r, const float *src, size_t count)
{
   size_t first = r->capacity - r->write_ptr;
   if (first > count)
      first = count;
   memcpy(r->buffer + r->write_ptr, src, first * sizeof(float));
   memcpy(r->buffer, src + first, (count - first) * sizeof(float));
   r->write_ptr = coreaudio_ring_advance(r->write_ptr, count, r->capacity);
   r->filled   += count;
}

static void ring_read(ring_t *r, float *dst, size_t count)
{
   size_t first = r->capacity - r->read_ptr;
   if (first > count)
      first = count;
   memcpy(dst, r->buffer + r->read_ptr, first * sizeof(float));
   memcpy(dst + first, r->buffer, (count - first) * sizeof(float));
   r->read_ptr = coreaudio_ring_advance(r->read_ptr, count, r->capacity);
   r->filled  -= count;
}

/* Every sample carries its channel index in the fraction, so a rotation
 * is visible in the value itself and not only in the ordering. */
static float sample_of(size_t frame, unsigned ch)
{
   return (float)frame + (float)ch / 100.0f;
}

/* Pushes many periods through the ring and checks each one arrives with
 * its channels in the order it was written in. */
static void layout_case(const char *name, unsigned channels,
      size_t period_frames, size_t ring_frames)
{
   ring_t   r;
   float   *in, *out;
   size_t   frame     = 0;
   size_t   cap       = ring_capacity(ring_frames, channels);
   size_t   period    = period_frames * channels;
   size_t   pass;
   int      rotated   = 0;
   int      wrapped   = 0;

   /* A capacity that is not a whole number of frames is noted, not
    * failed: case 1 shows which layouts that is, and the point of this
    * case is that it makes no difference to what comes out. */

   memset(&r, 0, sizeof(r));
   r.capacity = cap;
   r.buffer   = (float*)calloc(cap, sizeof(float));
   in         = (float*)calloc(period, sizeof(float));
   out        = (float*)calloc(period, sizeof(float));

   /* Enough passes that the cursors go round several times. */
   for (pass = 0; pass < (cap / period + 4) * 5; pass++)
   {
      size_t i, c;
      size_t before = r.write_ptr;

      for (i = 0; i < period_frames; i++)
         for (c = 0; c < channels; c++)
            in[i * channels + c] = sample_of(frame + i, (unsigned)c);

      if (r.capacity - r.filled < period)
      {
         free(r.buffer); free(in); free(out);
         fail(name, "the ring ran out of room, which the sizing should prevent");
         return;
      }

      ring_write(&r, in, period);
      if (r.write_ptr < before)
         wrapped = 1;
      ring_read(&r, out, period);

      for (i = 0; i < period_frames; i++)
         for (c = 0; c < channels; c++)
         {
            float want = sample_of(frame + i, (unsigned)c);
            float got  = out[i * channels + c];
            if (want != got)
            {
               if (!rotated)
               {
                  char d[160];
                  snprintf(d, sizeof(d),
                        "frame %u channel %u came back as %.2f, wanted %.2f"
                        " (capacity %u samples, %u channels)",
                        (unsigned)(frame + i), (unsigned)c, got, want,
                        (unsigned)cap, channels);
                  fail(name, d);
               }
               rotated = 1;
            }
         }

      frame += period_frames;
   }

   if (!wrapped)
      fail(name, "the cursors never wrapped, so this proved nothing");
   else if (!rotated)
      printf("   ok   %-10s %u channels, capacity %u samples (%u frames),"
             " wrapped, channels intact\n",
            name, channels, (unsigned)cap, (unsigned)(cap / channels));

   free(r.buffer);
   free(in);
   free(out);
}

/* Positions the cursors so that every single transfer crosses the
 * wrap, which is where a mid-frame wrap would do its damage if it did
 * any. */
static void straddle_case(const char *name, unsigned channels,
      size_t ring_frames)
{
   ring_t  r;
   float  *in, *out;
   size_t  cap    = ring_capacity(ring_frames, channels);
   size_t  period = 64 * channels;
   size_t  frame  = 0;
   size_t  pass;
   int     bad    = 0;

   memset(&r, 0, sizeof(r));
   r.capacity  = cap;
   r.buffer    = (float*)calloc(cap, sizeof(float));
   in          = (float*)calloc(period, sizeof(float));
   out         = (float*)calloc(period, sizeof(float));
   /* Start a half period short of the end, so the first write wraps
    * and, the period dividing nothing in particular, so does each one
    * after it. */
   r.write_ptr = r.read_ptr = cap - period / 2;

   for (pass = 0; pass < 400; pass++)
   {
      size_t i, c;
      for (i = 0; i < 64; i++)
         for (c = 0; c < channels; c++)
            in[i * channels + c] = sample_of(frame + i, (unsigned)c);
      ring_write(&r, in, period);
      ring_read(&r, out, period);
      for (i = 0; i < 64 && !bad; i++)
         for (c = 0; c < channels; c++)
            if (out[i * channels + c] != sample_of(frame + i, (unsigned)c))
            {
               char d[128];
               snprintf(d, sizeof(d),
                     "frame %u channel %u came back as %.2f",
                     (unsigned)(frame + i), (unsigned)c,
                     out[i * channels + c]);
               fail(name, d);
               bad = 1;
               break;
            }
      frame += 64;
   }
   if (!bad)
      printf("   ok   %-14s %u channels, 400 transfers all crossing the wrap\n",
            name, channels);
   free(r.buffer); free(in); free(out);
}

/* The hazard that is real: a read of a part-frame. Every frame after
 * it is skewed by the remainder, and it never comes back. The driver
 * trims short reads to whole frames to prevent exactly this; if that
 * trim is ever lost, this is what it looks like. */
static void misaligned_case(unsigned channels)
{
   ring_t  r;
   float  *in, *out;
   size_t  cap    = ring_capacity(2048, channels);
   size_t  period = 64 * channels;
   size_t  i, c;
   int     skewed = 0;

   memset(&r, 0, sizeof(r));
   r.capacity = cap;
   r.buffer   = (float*)calloc(cap, sizeof(float));
   in         = (float*)calloc(period * 2, sizeof(float));
   out        = (float*)calloc(period * 2, sizeof(float));

   for (i = 0; i < 128; i++)
      for (c = 0; c < channels; c++)
         in[i * channels + c] = sample_of(i, (unsigned)c);
   ring_write(&r, in, period * 2);

   /* One sample short of a frame, which is what an untrimmed short
    * read would do. */
   ring_read(&r, out, period - 1);
   ring_read(&r, out, period);

   for (i = 0; i < 64; i++)
      for (c = 0; c < channels; c++)
         if (out[i * channels + c] != sample_of(64 + i, (unsigned)c))
            skewed = 1;

   if (skewed)
      printf("   ok   %u channels: a part-frame read skews everything after"
             " it, as it must\n", channels);
   else
      fail("misaligned", "a part-frame read went unnoticed, so this test"
            " no longer guards the trim");

   free(r.buffer); free(in); free(out);
}

int main(void)
{
   unsigned layouts[] = { 1, 2, 4, 6, 8 };
   unsigned i;

   printf("1. what the power-of-two capacity does per layout\n");
   for (i = 0; i < sizeof(layouts) / sizeof(*layouts); i++)
   {
      unsigned ch  = layouts[i];
      size_t   cap = ring_capacity(682, ch);
      printf("   %u channels: %u samples, %u frames and %u over%s\n",
            ch, (unsigned)cap, (unsigned)(cap / ch), (unsigned)(cap % ch),
            (cap % ch) ? "  <- wraps mid-frame" : "");
   }
   printf("   (6 channels is the one that does not divide; cases 3 and 4\n"
          "    are whether that reaches the device, and it does not)\n");

   printf("3. traffic through the ring, channels checked on the way out\n");
   layout_case("mono",    1, 512, 2048);
   layout_case("stereo",  2, 512, 2048);
   layout_case("quad",    4, 512, 2048);
   layout_case("5.1",     6, 512, 2048);
   layout_case("7.1",     8, 512, 2048);
   /* Period sizes that do not divide the capacity, so the wrap lands
    * somewhere different each time round. */
   layout_case("5.1 odd", 6, 300, 2048);
   layout_case("7.1 odd", 8, 300, 2048);
   layout_case("5.1 tiny", 6,  17, 1024);

   printf("4. every transfer straddling the wrap, on purpose\n");
   straddle_case("5.1 straddle", 6, 2048);
   straddle_case("7.1 straddle", 8, 2048);

   printf("5. a transfer that is not a whole number of frames\n");
   misaligned_case(6);
   misaligned_case(8);

   printf("6. advance never lands outside the ring\n");
   {
      size_t cap = ring_capacity(64, 6);
      size_t ptr = 0, n;
      for (n = 0; n < 10000; n++)
      {
         size_t count = (n * 37) % (cap + 1);
         ptr = coreaudio_ring_advance(ptr, count, cap);
         if (ptr >= cap)
         {
            fail("advance", "cursor left the ring");
            break;
         }
      }
      if (ptr < cap)
         printf("   ok   cursor stayed inside for 10000 advances\n");
   }

   if (failures)
   {
      printf("coreaudio ring: %d failure(s)\n", failures);
      return 1;
   }
   printf("coreaudio ring: frames survive the wrap in every layout;"
          " a part-frame count is what breaks them\n");
   return 0;
}

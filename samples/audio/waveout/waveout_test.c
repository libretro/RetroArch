/* The waveout audio driver, run for real: under Wine with a sound
 * device (a silent one will do), it is opened, written three seconds
 * of sound, and held to what the audio interface says of a driver:
 *
 * - a blocking write takes all it is given, and three seconds of sound
 *   is not written in less: the device sets the pace, not the driver.
 *   (How much more than three seconds it takes says nothing of the
 *   driver - a silent device under Wine starts half a second late and
 *   was measured playing three seconds of sound, queued whole with no
 *   driver in between, in 3.9 - so the test allows up to six.)
 * - what it says it will take now is never more than its buffer, and
 *   is nothing when every block is with the device;
 * - the frames it says the device has consumed only go up, and end
 *   within a buffer of what was written;
 * - a write that must not block returns short rather than wait;
 * - stopped, it takes what its buffer holds and no more, without
 *   waiting; started again, it plays on.
 *
 * With no sound device the driver does not open; the test says so and
 * passes, since there is nothing to hold it to. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#include "../../../audio/drivers/waveout.c"

/* what the driver logs through */
void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

#define RATE    48000
#define LATENCY 64

int main(void)
{
   static int16_t chunk[480 * 2];           /* 10 ms */
   unsigned new_rate = 0;
   unsigned i, n;
   size_t buffer, written_frames = 0, last_consumed = 0, consumed;
   DWORD t0, t1;
   void *wo;

   wo = audio_waveout.init(NULL, RATE, LATENCY, &new_rate);
   if (!wo)
   {
      printf("[skip] waveout: no sound device to open\n");
      return 0;
   }
   for (i = 0; i < 480; i++)
   {
      chunk[i * 2]     = (int16_t)(8000.0 * sin(i * 2.0 * 3.14159265 / 48.0));
      chunk[i * 2 + 1] = chunk[i * 2];
   }

   buffer = audio_waveout.buffer_size(wo);
   CHECK(new_rate == RATE, "the rate came back as %u", new_rate);
   CHECK(buffer >= (size_t)RATE * LATENCY / 1000 * 4,
         "the buffer is %u bytes, less than the %u ms asked for",
         (unsigned)buffer, LATENCY);
   CHECK(audio_waveout.write_avail(wo) == buffer,
         "an empty driver says it will take %u of %u bytes",
         (unsigned)audio_waveout.write_avail(wo), (unsigned)buffer);
   CHECK(audio_waveout.alive(wo), "not alive after it opened");

   /* three seconds, blocking */
   t0 = GetTickCount();
   for (n = 0; n < 300; n++)
   {
      ssize_t w = audio_waveout.write(wo, chunk, sizeof(chunk));
      size_t avail;
      if (w != (ssize_t)sizeof(chunk))
      {
         CHECK(false, "blocking write %u took %d of %u bytes",
               n, (int)w, (unsigned)sizeof(chunk));
         break;
      }
      written_frames += 480;
      avail    = audio_waveout.write_avail(wo);
      consumed = audio_waveout.frames_consumed(wo);
      CHECK(avail <= buffer, "it will take %u bytes of a %u byte buffer",
            (unsigned)avail, (unsigned)buffer);
      CHECK(consumed >= last_consumed, "frames consumed went back, %u to %u",
            (unsigned)last_consumed, (unsigned)consumed);
      CHECK(consumed <= written_frames, "consumed %u frames of %u written",
            (unsigned)consumed, (unsigned)written_frames);
      last_consumed = consumed;
   }
   t1 = GetTickCount();
   fprintf(stderr, "   waveout: 3.00 s of sound written in %.2f s; the device has"
         " consumed %u of %u frames, the buffer is %u\n",
         (t1 - t0) / 1000.0, (unsigned)last_consumed,
         (unsigned)written_frames, (unsigned)(buffer / 4));
   CHECK(t1 - t0 > 2500 && t1 - t0 < 6000,
         "three seconds of sound took %.2f s to write", (t1 - t0) / 1000.0);
   CHECK(written_frames - last_consumed <= buffer / 4 + 480,
         "%u frames written and not consumed, with a buffer of %u",
         (unsigned)(written_frames - last_consumed), (unsigned)(buffer / 4));

   /* not to block: filled to the brim, it returns short and at once */
   audio_waveout.set_nonblock_state(wo, true);
   t0 = GetTickCount();
   for (n = 0, i = 0; n < 200; n++)
      if (audio_waveout.write(wo, chunk, sizeof(chunk)) < (ssize_t)sizeof(chunk))
         i++;
   t1 = GetTickCount();
   CHECK(i > 100, "of 200 writes that must not block, %u came back short", i);
   CHECK(t1 - t0 < 1000, "200 writes that must not block took %u ms",
         (unsigned)(t1 - t0));
   audio_waveout.set_nonblock_state(wo, false);

   /* room is waited for, for a bounded while */
   t0 = GetTickCount();
   n  = (unsigned)audio_waveout.wait_writable(wo, sizeof(chunk));
   t1 = GetTickCount();
   CHECK(n >= sizeof(chunk), "waited for room for %u bytes and got %u",
         (unsigned)sizeof(chunk), n);
   CHECK(t1 - t0 < 500, "waiting for room took %u ms", (unsigned)(t1 - t0));

   /* stopped: no waiting on a device that is not playing */
   CHECK(audio_waveout.stop(wo) && !audio_waveout.alive(wo), "it did not stop");
   t0 = GetTickCount();
   for (n = 0; n < 50; n++)
      audio_waveout.write(wo, chunk, sizeof(chunk));
   t1 = GetTickCount();
   CHECK(t1 - t0 < 500, "50 writes to a stopped device took %u ms",
         (unsigned)(t1 - t0));
   consumed = audio_waveout.frames_consumed(wo);
   CHECK(audio_waveout.start(wo, false) && audio_waveout.alive(wo), "it did not start again");
   Sleep(300);
   CHECK(audio_waveout.frames_consumed(wo) > consumed,
         "started again, the device consumed nothing in 300 ms");

   audio_waveout.free(wo);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS waveout_test\n");
   return 0;
}

/* Deterministic CoreAudio writer tests. run.py inserts the source functions.
 * Apple calls and the clock are stubbed; this does not test Apple hardware. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <boolean.h>
#include <retro_atomic.h>

typedef unsigned UInt32;
typedef int OSStatus;
typedef unsigned AudioUnitRenderActionFlags;
typedef int AudioTimeStamp;
typedef long long retro_time_t;
typedef struct { unsigned tv_sec, tv_nsec; } mach_timespec_t;
typedef struct { void *mData; size_t mDataByteSize; } AudioBuffer;
typedef struct { unsigned mNumberBuffers; AudioBuffer mBuffers[2]; } AudioBufferList;
typedef struct coreaudio
{
   float *buffer;
   size_t capacity, usable, read_ptr, write_ptr, period_pull, max_pull_frames;
   unsigned channels;
   retro_atomic_size_t filled, consumed, underruns, max_pull_observed;
   retro_atomic_size_t oversized_pulls, format_errors, worst_short, worst_short_avail;
   retro_atomic_int_t waiters;
   bool want_running, unit_running, is_paused, nonblock;
   struct coreaudio *dev, *sema;
} coreaudio_t;

#define noErr 0
#define kAudioUnitScope_Global 0
#define kAudioOutputUnitProperty_IsRunning 0
#define kAudioUnitRenderAction_OutputIsSilence 1

static retro_time_t simulated_usec;
static unsigned waits, failures, cases;
static bool stopped, frozen, start_fails, pause_on_wait;
static size_t played;
static float input[48000 * 6];
static float output[48000 * 6];
static int semaphore_signal(coreaudio_t *dev) { (void)dev; return 0; }
static int semaphore_timedwait(coreaudio_t *dev, mach_timespec_t ts);
static OSStatus AudioOutputUnitStart(coreaudio_t *dev)
{ (void)dev; return start_fails ? -1 : noErr; }
static OSStatus AudioUnitGetProperty(coreaudio_t *dev, unsigned prop,
      unsigned scope, unsigned bus, UInt32 *running, UInt32 *size)
{
   (void)dev; (void)prop; (void)scope; (void)bus; (void)size;
   *running = !stopped;
   return noErr;
}

/* DRIVER_FUNCTIONS */

static void render(coreaudio_t *dev)
{
   float data[3072 * 6];
   AudioBufferList list;
   AudioUnitRenderActionFlags flags = 0;
   size_t n = dev->period_pull * dev->channels;
   list.mNumberBuffers = 1;
   list.mBuffers[0].mData = data;
   list.mBuffers[0].mDataByteSize = n * sizeof(float);
   coreaudio_audio_write_cb(dev, &flags, NULL, 0,
         (UInt32)dev->period_pull, &list);
   if (played + n <= sizeof(output) / sizeof(output[0]))
      memcpy(output + played, data, n * sizeof(float));
   played += n;
}

static int semaphore_timedwait(coreaudio_t *dev, mach_timespec_t ts)
{
   waits++;
   if (waits > 10000)
   {
      puts("FAIL: unbounded wait");
      exit(1);
   }
   if (pause_on_wait)
      dev->is_paused = true;
   if (frozen || !dev->unit_running)
      simulated_usec += (retro_time_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
   else
   {
      simulated_usec += dev->period_pull * 1000000 / 48000;
      render(dev);
   }
   return 0;
}

static void init(coreaudio_t *dev, unsigned ms, unsigned channels)
{
   memset(dev, 0, sizeof(*dev));
   simulated_usec = 0; waits = 0; played = 0;
   stopped = frozen = start_fails = pause_on_wait = false;
   dev->usable = ms * 48 * channels;
   dev->capacity = 1;
   while (dev->capacity < dev->usable)
      dev->capacity <<= 1;
   dev->buffer = (float*)calloc(dev->capacity, sizeof(float));
   if (!dev->buffer) exit(2);
   dev->channels = channels;
   dev->period_pull = ms * 48 / 4;
   dev->max_pull_frames = dev->period_pull;
   dev->want_running = true;
   dev->dev = dev->sema = dev;
   retro_atomic_size_init(&dev->filled, 0);
   retro_atomic_size_init(&dev->consumed, 0);
   retro_atomic_size_init(&dev->underruns, 0);
   retro_atomic_size_init(&dev->max_pull_observed, 0);
   retro_atomic_size_init(&dev->oversized_pulls, 0);
   retro_atomic_size_init(&dev->format_errors, 0);
   retro_atomic_size_init(&dev->worst_short, 0);
   retro_atomic_size_init(&dev->worst_short_avail, 0);
   retro_atomic_int_init(&dev->waiters, 0);
}

#define CHECK(test, label) do { cases++; if (!(test)) { \
   printf("FAIL: %s (waits=%u, elapsed=%lld us)\n", label, waits, simulated_usec); \
   failures++; } } while (0)

static void batch(unsigned ms, unsigned channels, unsigned frames)
{
   coreaudio_t dev;
   size_t bytes = frames * channels * sizeof(float);
   size_t result;
   char label[100];
   init(&dev, ms, channels);
   result = (size_t)coreaudio_write(&dev, input, bytes);
   sprintf(label, "%u ms / %u channels / %u frames: full write", ms, channels, frames);
   CHECK(result == bytes, label);
   while (retro_atomic_load_acquire_size(&dev.filled))
      render(&dev);
   CHECK(played >= frames * channels && !memcmp(input, output, bytes),
         "all input samples played in order across wraparound");
   free(dev.buffer);
}

static void short_buffer(unsigned channels, bool underrun)
{
   coreaudio_t dev;
   float data[32], next[32];
   AudioBufferList list;
   AudioUnitRenderActionFlags flags = 0;
   size_t bytes = (2 * channels - 1) * sizeof(float) + 1;
   size_t queued = (underrun ? 0 : 4) * channels;
   size_t i;

   init(&dev, 8, channels);
   if (queued)
      rb_write(&dev, input, queued);
   memset(data, 0x5a, sizeof(data));
   list.mNumberBuffers = 1;
   list.mBuffers[0].mData = data;
   list.mBuffers[0].mDataByteSize = bytes;
   coreaudio_audio_write_cb(&dev, &flags, NULL, 0, 2, &list);
   CHECK(dev.read_ptr == (underrun ? 0 : channels),
         "undersized output consumes whole frames only");
   CHECK(underrun || !memcmp(data, input, channels * sizeof(float)),
         "undersized output preserves complete frame");
   for (i = channels * sizeof(float); i < bytes; i++)
      if (((unsigned char*)data)[i])
         break;
   CHECK(i == bytes, "incomplete output frame and trailing byte are silent");
   CHECK(((unsigned char*)data)[bytes] == 0x5a, "output respects byte capacity");
   list.mBuffers[0].mData = next;
   list.mBuffers[0].mDataByteSize = channels * sizeof(float);
   coreaudio_audio_write_cb(&dev, &flags, NULL, 0, 1, &list);
   CHECK(underrun || !memcmp(next, input + channels, channels * sizeof(float)),
         "next callback retains channel alignment");
   free(dev.buffer);
}

static void partial_byte_request(unsigned channels)
{
   coreaudio_t dev;
   init(&dev, 8, channels);
   rb_write(&dev, input, dev.usable - channels);
   frozen = true;
   CHECK(coreaudio_wait_writable(&dev, channels * sizeof(float) + 1) == 0,
         "one byte beyond a free frame needs another complete frame");
   CHECK(waits == 8, "unaligned byte request obeys the existing bounded wait");
   free(dev.buffer);
}

static void fragment_buffer(unsigned channels)
{
   coreaudio_t dev;
   float data[8];
   AudioBufferList list;
   AudioUnitRenderActionFlags flags = 0x100;
   size_t bytes = channels * sizeof(float) - 1;
   size_t i;
   init(&dev, 8, channels);
   rb_write(&dev, input, channels);
   memset(data, 0x5a, sizeof(data));
   list.mNumberBuffers = 1;
   list.mBuffers[0].mData = data;
   list.mBuffers[0].mDataByteSize = bytes;
   coreaudio_audio_write_cb(&dev, &flags, NULL, 0, 1, &list);
   CHECK(dev.read_ptr == 0, "fragment-only output preserves the complete frame");
   for (i = 0; i < bytes; i++)
      if (((unsigned char*)data)[i])
         break;
   CHECK(i == bytes, "fragment-only output is fully silenced");
   CHECK(((unsigned char*)data)[bytes] == 0x5a, "fragment output respects capacity");
   CHECK(flags == (0x100 | kAudioUnitRenderAction_OutputIsSilence),
         "fragment output preserves flags and marks silence");
   CHECK(retro_atomic_load_acquire_size(&dev.format_errors) == 1,
         "short output storage records a format error");
   free(dev.buffer);
}

int main(int argc, char **argv)
{
   coreaudio_t dev;
   unsigned i, j, k;
   static const unsigned latencies[] = {8, 16, 64};
   static const unsigned frames[] = {800, 1600, 3200, 48000};
   size_t result;
   for (i = 0; i < sizeof(input) / sizeof(input[0]); i++)
      input[i] = (float)(i + 1);
   if (argc > 1 && !strcmp(argv[1], "null-output"))
   {
      AudioBufferList list;
      AudioUnitRenderActionFlags flags = 0;
      init(&dev, 8, 2);
      rb_write(&dev, input, 4);
      list.mNumberBuffers = 1;
      list.mBuffers[0].mData = NULL;
      list.mBuffers[0].mDataByteSize = 16;
      coreaudio_audio_write_cb(&dev, &flags, NULL, 0, 2, &list);
      CHECK(dev.read_ptr == 0, "missing output storage preserves queued audio");
      CHECK((flags & kAudioUnitRenderAction_OutputIsSilence) != 0,
            "missing output storage marks silence");
      CHECK(retro_atomic_load_acquire_size(&dev.format_errors) == 1,
            "missing output storage records a format error");
      free(dev.buffer);
      printf("%u checks, %u failures (null output)\n", cases, failures);
      return failures ? 1 : 0;
   }
   for (j = 2; j <= 6; j += 4)
   {
      short_buffer(j, false);
      short_buffer(j, true);
      partial_byte_request(j);
      fragment_buffer(j);
   }
   for (i = 0; i < 3; i++)
      for (j = 2; j <= 6; j += 4)
         for (k = 0; k < 4; k++)
            batch(latencies[i], j, frames[k]);

   init(&dev, 8, 2);
   frozen = true;
   result = (size_t)coreaudio_write(&dev, input, 800 * 2 * sizeof(float));
   CHECK(result == dev.usable * sizeof(float) && simulated_usec >= 800000 && simulated_usec <= 900000,
         "writer times out when running device makes no progress");
   waits = 0; simulated_usec = 0;
   CHECK(coreaudio_wait_writable(&dev, sizeof(float) * 2) == 0
         && simulated_usec >= 800000 && simulated_usec <= 900000, "writable wait has a bounded stall timeout");
   free(dev.buffer);

   init(&dev, 8, 2);
   start_fails = true;
   result = (size_t)coreaudio_write(&dev, input, 800 * 2 * sizeof(float));
   CHECK(result == dev.usable * sizeof(float) && simulated_usec <= 900000,
         "failed unit start cannot hang writer");
   free(dev.buffer);

   init(&dev, 8, 2);
   dev.nonblock = true;
   result = (size_t)coreaudio_write(&dev, input, 800 * 2 * sizeof(float));
   CHECK(result == dev.usable * sizeof(float) && waits == 0,
         "nonblocking writes return immediately with accepted bytes");
   stopped = true;
   CHECK(coreaudio_wait_writable(&dev, 8) == 0 && waits == 0,
         "stopped unit returns immediately");
   free(dev.buffer);

   init(&dev, 8, 2);
   pause_on_wait = true;
   result = (size_t)coreaudio_write(&dev, input, 800 * 2 * sizeof(float));
   CHECK(result == dev.usable * sizeof(float) && waits == 1,
         "pause interrupts writer");
   CHECK(coreaudio_wait_writable(&dev, 8) == 0, "paused unit cannot be writable");
   free(dev.buffer);

   printf("%u checks, %u failures\n", cases, failures);
   return failures ? 1 : 0;
}

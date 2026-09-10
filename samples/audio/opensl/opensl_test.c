/* audio/drivers/opensl.c against a scripted OpenSL player.
 *
 * Issue #6405: certain audio-latency values hung the app on Android.
 * The reporter worked out why - at a fixed 1024-frame block, a
 * latency of 34 ms or less made the block count round to one, and a
 * queue of one block wedged the write. The driver has since gained
 * the floor the reporter asked for and a block size that follows the
 * latency, and its waits are bounded; none of that had a test, and a
 * phone is the only place it runs.
 *
 * So: the block arithmetic at the reporter's own numbers and below,
 * the queue accounting the hang came from, and a device that stops
 * consuming - which is the other way a write can never return. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"
#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_opensl;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t pcm[65536];
extern unsigned stub_device_block_frames;

/* The reporter's case: 44.1 kHz, a 1024-frame block from the device,
 * and the latency swept through the value where it used to fail. */
static void latency_case(unsigned latency, unsigned burst, unsigned rate)
{
   void *h;
   unsigned new_rate = 0;
   size_t   frame_bytes, total;

   opensl_mock_reset();
   /* The platform's fact, not a setting: the driver asks for it. */
   stub_device_block_frames = burst;
   h = audio_opensl.init(NULL, rate, latency, &new_rate);
   CHECK(h != NULL, "latency %u: init failed", latency);
   if (!h)
      return;
   frame_bytes = audio_opensl.use_float(h) ? 8 : 4;
   total       = audio_opensl.buffer_size(h);
   printf("      latency %2u ms, device burst %4u frames: %u buffer(s) of %u bytes, %.1f ms total\n",
         latency, burst, opensl_mock_num_buffers(),
         (unsigned)(total / opensl_mock_num_buffers()),
         total / frame_bytes * 1000.0 / rate);
   /* The floor the reporter asked for: a queue of one block cannot be
    * written to at all, since the block being filled is the one the
    * device is playing. */
   CHECK(opensl_mock_num_buffers() >= 2,
         "latency %u gave the device %u buffer(s)", latency, opensl_mock_num_buffers());
   CHECK(total > 0 && total % frame_bytes == 0, "buffer_size %u is not whole frames", (unsigned)total);
   if (burst)
   {
      /* Every block a whole burst: a queue that is not a multiple of
       * the device's burst leaves the fast mixer, which costs
       * latency rather than saving it. */
      size_t block = total / opensl_mock_num_buffers();
      CHECK(block % (burst * frame_bytes) == 0,
            "latency %u: a %u-byte block is not a multiple of the device's %u-frame burst",
            latency, (unsigned)block, burst);
   }
   /* Writes go through and the device plays them. */
   {
      size_t i, sent = 0;
      audio_opensl.set_nonblock_state(h, false);
      audio_opensl.start(h, false);
      for (i = 0; i < 24; i++)
      {
         ssize_t w = audio_opensl.write(h, pcm, total / 4);
         CHECK(w > 0, "latency %u: write %u returned %ld", latency, (unsigned)i, (long)w);
         if (w > 0) sent += w;
      }
      usleep(120000);
      printf("      %u bytes written, device played %u block(s)\n",
            (unsigned)sent, (unsigned)opensl_mock_consumed());
      CHECK(opensl_mock_consumed() > 0, "latency %u: the device played nothing", latency);
      CHECK(opensl_mock_enqueue_failures() == 0,
            "latency %u: %u block(s) were pushed at a full queue",
            latency, opensl_mock_enqueue_failures());
   }
   audio_opensl.free(h);
   CHECK(opensl_mock_objects() == 0, "latency %u: %d object(s) left alive", latency, opensl_mock_objects());
}

int main(void)
{
   void *h;
   unsigned new_rate = 0;

   memset(pcm, 0x11, sizeof(pcm));
   printf("opensl:\n");

   printf("   the latencies from issue #6405, at the device's 1024-frame burst\n");
   latency_case(35, 1024, 44100);
   latency_case(34, 1024, 44100);   /* the one that used to wedge */
   latency_case(16, 1024, 44100);
   latency_case(8,  1024, 44100);
   latency_case(1,  1024, 44100);

   printf("   a phone's own burst: the latency is reachable down to two of them\n");
   latency_case(64, 192, 48000);
   latency_case(16, 192, 48000);
   latency_case(8,  192, 48000);   /* 8 ms is two bursts: the floor, and reached */
   latency_case(64, 240, 44100);
   latency_case(8,  240, 44100);

   printf("   a device that reports no burst: the block follows the latency\n");
   latency_case(64, 0, 48000);
   latency_case(16, 0, 48000);
   latency_case(8,  0, 48000);

   printf("   float where the device takes it, 16-bit where it does not\n");
   opensl_mock_reset();
   h = audio_opensl.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      CHECK(audio_opensl.use_float(h) == (opensl_mock_is_float() != 0),
            "use_float says %d, the player took %s",
            (int)audio_opensl.use_float(h), opensl_mock_is_float() ? "float" : "16-bit");
      audio_opensl.free(h);
   }
   opensl_mock_reset();
   opensl_mock_set_float_supported(0);
   h = audio_opensl.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on a 16-bit-only device");
   if (h)
   {
      CHECK(!audio_opensl.use_float(h), "float was reported on a device that refused it");
      CHECK(!opensl_mock_is_float(), "the player was created with a float format anyway");
      audio_opensl.free(h);
   }

   printf("   a full queue: write_avail is nil, not an enormous number\n");
   opensl_mock_reset();
   h = audio_opensl.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      size_t total = audio_opensl.buffer_size(h);
      /* Every block is with the device from the start: the driver
       * primes the queue full. write_avail has to say so - it is a
       * divisor and a setpoint for rate control, and an unsigned
       * underflow there reads as a buffer that is always empty. */
      size_t avail = audio_opensl.write_avail(h);
      printf("      primed: write_avail %u of a %u-byte buffer\n", (unsigned)avail, (unsigned)total);
      CHECK(avail <= total, "write_avail reports %u of a %u-byte buffer", (unsigned)avail, (unsigned)total);
      audio_opensl.free(h);
   }

   printf("   a device that stops consuming: the write comes back rather than hanging\n");
   opensl_mock_reset();
   h = audio_opensl.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      size_t total = audio_opensl.buffer_size(h);
      ssize_t w;
      unsigned i;
      audio_opensl.set_nonblock_state(h, false);
      audio_opensl.start(h, false);
      opensl_mock_freeze(1);
      /* more than the queue can hold, at a device that plays nothing */
      for (i = 0; i < 8; i++)
      {
         w = audio_opensl.write(h, pcm, total);
         if (w <= 0)
            break;
      }
      printf("      the %u%s write to a frozen device returned %ld\n",
            i + 1, i == 0 ? "st" : "th", (long)w);
      CHECK(w >= 0 && w < (ssize_t)total, "a frozen device took %ld of %u bytes", (long)w, (unsigned)total);
      CHECK(audio_opensl.wait_writable(h, total / 2) == 0,
            "wait_writable found room at a frozen device");
      opensl_mock_freeze(0);
      audio_opensl.free(h);
   }

   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("opensl: the block count has a floor, the queue accounts, and no wait is unbounded\n");
   return 0;
}

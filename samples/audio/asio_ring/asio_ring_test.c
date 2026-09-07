/* ASIO ring sizing, against the devices its floor was tuned on and
 * the one that showed the old floor scaling the wrong way.
 *
 * audio/drivers/asio_ring.h sizes the ring in front of the device from
 * the latency setting less the device stage, floored at four periods
 * where that is under 6 ms, 6 ms where it is over, and two periods at
 * the least. Four periods alone was 5 ms on ASIO4ALL at 64 frames and
 * 44 ms on a wrapper locked at 528, four times the setting on a device
 * that needs 22. The expected frames here are worked by hand from the
 * three rules, not from the code. */

#include <stdio.h>

#include "../../../audio/drivers/asio_ring.h"

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

int main(void)
{
   /* A tiny period: four of it, well under the cap. */
   CHECK(asio_ring_floor_frames(48000, 8) == 32,
         "floor at 8 frames: %u", (unsigned)asio_ring_floor_frames(48000, 8));
   /* ASIO4ALL at its minimum: four periods, 256, as it always was. */
   CHECK(asio_ring_floor_frames(48000, 64) == 256,
         "floor at 64 frames: %u", (unsigned)asio_ring_floor_frames(48000, 64));
   /* Where four periods meet the 6 ms cap, 288 frames at 48 kHz. */
   CHECK(asio_ring_floor_frames(48000, 72) == 288,
         "floor at 72 frames: %u", (unsigned)asio_ring_floor_frames(48000, 72));
   CHECK(asio_ring_floor_frames(48000, 73) == 288,
         "floor at 73 frames: %u", (unsigned)asio_ring_floor_frames(48000, 73));
   /* Capped: 128 frames gets the 288, not 512. */
   CHECK(asio_ring_floor_frames(48000, 128) == 288,
         "floor at 128 frames: %u", (unsigned)asio_ring_floor_frames(48000, 128));
   /* Where the cap meets two periods: 144 is 288 either way, and one
    * more frame lifts it. */
   CHECK(asio_ring_floor_frames(48000, 144) == 288,
         "floor at 144 frames: %u", (unsigned)asio_ring_floor_frames(48000, 144));
   CHECK(asio_ring_floor_frames(48000, 145) == 290,
         "floor at 145 frames: %u", (unsigned)asio_ring_floor_frames(48000, 145));
   /* A wrapper locked at 528: two periods, 1056 (22 ms), where four
    * gave 2112 (44 ms). */
   CHECK(asio_ring_floor_frames(48000, 528) == 1056,
         "floor at 528 frames: %u", (unsigned)asio_ring_floor_frames(48000, 528));
   /* The cap follows the rate. */
   CHECK(asio_ring_floor_frames(44100, 128) == 264,
         "floor at 44.1 kHz: %u", (unsigned)asio_ring_floor_frames(44100, 128));
   CHECK(asio_ring_floor_frames(96000, 128) == 512,
         "floor at 96 kHz: %u", (unsigned)asio_ring_floor_frames(96000, 128));

   /* The 8 ms setting on ASIO4ALL over HDMI: 384 frames of setting
    * against a 616-frame device stage is nothing left for the ring, so
    * the floor, four periods. */
   CHECK(asio_ring_frames_for(48000, 8, 616, 64) == 256,
         "8 ms, 616-frame device, 64-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 8, 616, 64));
   /* The same setting on the 528-frame wrapper: the floor again, now
    * two periods. */
   CHECK(asio_ring_frames_for(48000, 8, 528, 528) == 1056,
         "8 ms, 528-frame device, 528-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 8, 528, 528));
   /* A setting with room in it: 64 ms is 3072 frames, less the 616
    * device stage, 2456 - above any floor. */
   CHECK(asio_ring_frames_for(48000, 64, 616, 64) == 2456,
         "64 ms, 616-frame device, 64-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 64, 616, 64));
   /* A setting that leaves less than the floor: 16 ms is 768 frames,
    * less 616 is 152, under 256. */
   CHECK(asio_ring_frames_for(48000, 16, 616, 64) == 256,
         "16 ms, 616-frame device, 64-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 16, 616, 64));
   /* The Topping at an 8-frame period, 72-frame device stage: 384 less
    * 72, 312, above a 32-frame floor. */
   CHECK(asio_ring_frames_for(48000, 8, 72, 8) == 312,
         "8 ms, 72-frame device, 8-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 8, 72, 8));
   /* No device stage known yet (two periods stand in for it in the
    * driver): the setting whole, above the floor. */
   CHECK(asio_ring_frames_for(48000, 32, 128, 64) == 1408,
         "32 ms, 128-frame device, 64-frame period: %u",
         (unsigned)asio_ring_frames_for(48000, 32, 128, 64));

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("asio ring: the floor is four periods to 6 ms and two at the least, the setting less the device stage above it\n");
   return 0;
}

/* camera/drivers/ffmpeg.c against a synthetic camera.
 *
 * The driver reads from a libavdevice input format. Built with that
 * format set to lavfi and handed a filter description as its device,
 * the whole pipeline runs with no camera attached: avformat opens the
 * source, the poll thread reads packets, the decoder decodes them,
 * swscale converts into the target buffer and the frontend's poll
 * takes the newest finished one.
 *
 * That is what this drives. The cases are the ones the driver's
 * threading has to get right and that no eyeball on a webcam would
 * show: start and stop repeatedly, poll while the thread is running,
 * stop while it is mid-frame, free without stopping first, see the
 * last frame decoded, and take a frame slowly without holding the
 * thread up. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <libretro.h>
#include "../../../camera/camera_driver.h"

extern camera_driver_t camera_ffmpeg;

static unsigned failures = 0;
static const char *current = "";
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL [%s]: ", current); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* The frontend's side of a poll: the driver hands over a frame. */
static unsigned frames_seen;
static unsigned last_width, last_height;
static uint32_t checksum;
static int last_px = -1;          /* blue of the first pixel */
static useconds_t cb_sleep_us;    /* a core slow to take its frame */

static void frame_raw_cb(const uint32_t *buffer, unsigned width,
      unsigned height, size_t pitch)
{
   unsigned x, y;
   frames_seen++;
   last_width  = width;
   last_height = height;
   if (!buffer)
      return;
   last_px = (int)(buffer[0] & 0xff);
   if (cb_sleep_us)
      usleep(cb_sleep_us);
   /* Touch every pixel: a buffer that was freed or is the wrong size
    * shows here under ASan rather than as a wrong-looking picture. */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
         checksum += buffer[y * (pitch / sizeof(uint32_t)) + x];
}

static void frame_gl_cb(unsigned texture_id, unsigned texture_target,
      const float *affine)
{
   (void)texture_id;
   (void)texture_target;
   (void)affine;
}

#define CAPS (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)
#define SRC  "testsrc=size=320x240:rate=30"

static void *open_camera(void)
{
   return camera_ffmpeg.init(SRC, CAPS, VIDEO_SCALE_PACK(320, 240));
}

/* Polls until a frame arrives or the patience runs out. The first
 * frame takes as long as the source takes to produce one. */
static bool poll_until_frame(void *h, unsigned tries)
{
   unsigned before = frames_seen;
   while (tries--)
   {
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
      if (frames_seen > before && last_width)
         return true;
      usleep(20000);
   }
   return false;
}

int main(void)
{
   void *h;
   unsigned i;

   printf("camera ffmpeg (lavfi):\n");

   printf("   a camera opens, runs and closes\n");
   current = "open/run/close";
   h = open_camera();
   CHECK(h != NULL, "the driver would not open a lavfi source");
   if (!h)
   {
      printf("%u failure(s)\n", failures + 1);
      return 1;
   }
   CHECK(camera_ffmpeg.start(h), "start failed");
   CHECK(poll_until_frame(h, 100), "no frame arrived");
   printf("      %u frame(s), %ux%u\n", frames_seen, last_width, last_height);
   CHECK(last_width == 320 && last_height == 240,
         "a frame of %ux%u, not the 320x240 asked for", last_width, last_height);
   camera_ffmpeg.stop(h);
   camera_ffmpeg.free(h);

   printf("   stop and start again, three times over\n");
   current = "restart";
   h = open_camera();
   CHECK(h != NULL, "the driver would not reopen");
   if (h)
   {
      for (i = 0; i < 3; i++)
      {
         unsigned before = frames_seen;
         CHECK(camera_ffmpeg.start(h), "start %u failed", i);
         CHECK(poll_until_frame(h, 100), "no frame after start %u", i);
         camera_ffmpeg.stop(h);
         printf("      cycle %u: %u frame(s)\n", i, frames_seen - before);
      }
      camera_ffmpeg.free(h);
   }

   /* stop() joins the poll thread and then flushes the decoder. Called
    * while the thread is mid-frame - which is every time, since the
    * source never stops - that ordering is the whole of it: a flush
    * against a live decoding thread is two users of one
    * AVCodecContext. */
   printf("   stop while the thread is mid-frame\n");
   current = "stop under load";
   for (i = 0; i < 5; i++)
   {
      h = open_camera();
      if (!h)
      {
         CHECK(0, "the driver would not open on cycle %u", i);
         break;
      }
      CHECK(camera_ffmpeg.start(h), "start failed on cycle %u", i);
      /* Increasing amounts of work before the stop, so the thread is
       * at a different point each time. */
      usleep(i * 5000);
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }
   printf("      five cycles, stopped after 0 to 20 ms\n");

   printf("   free without stopping\n");
   current = "free while running";
   h = open_camera();
   CHECK(h != NULL, "the driver would not open");
   if (h)
   {
      CHECK(camera_ffmpeg.start(h), "start failed");
      poll_until_frame(h, 50);
      /* No stop(): free has to do it. A host that drops content
       * without stopping the camera does exactly this. */
      camera_ffmpeg.free(h);
      printf("      freed with the thread still running\n");
   }

   /* A source of eight frames whose grey level is 16 per frame: once
    * the thread has decoded the last, a poll hands out that one - the
    * frame just finished, not the one before it. */
   printf("   a poll hands out the newest finished frame\n");
   current = "newest frame";
   h = camera_ffmpeg.init(
         "color=c=black:size=320x240:rate=25,format=gray,geq=lum='N*16',trim=end_frame=8",
         CAPS, VIDEO_SCALE_PACK(320, 240));
   CHECK(h != NULL, "the driver would not open the counting source");
   if (h)
   {
      int seen = -1, stable = 0;
      CHECK(camera_ffmpeg.start(h), "start failed");
      for (i = 0; i < 300 && stable < 15; i++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
         if (last_px == seen)
            stable++;
         else
            stable = 0;
         seen = last_px;
         usleep(20000);
      }
      printf("      settled on grey %d\n", seen);
      CHECK(seen >= 7 * 16 - 4 && seen <= 7 * 16 + 4,
            "the last frame is grey %d, the poll shows %d", 7 * 16, seen);
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }

   /* A core that takes its time over a frame: the thread goes on
    * decoding meanwhile, so the next poll is many frames on. A thread
    * held off for the length of the callback would be one frame on at
    * most. */
   printf("   a slow poll does not hold the thread up\n");
   current = "slow poll";
   h = camera_ffmpeg.init(
         "color=c=black:size=320x240:rate=100,format=gray,geq=lum='mod(N,256)',realtime",
         CAPS, VIDEO_SCALE_PACK(320, 240));
   CHECK(h != NULL, "the driver would not open the paced source");
   if (h)
   {
      int before, after, moved;
      CHECK(camera_ffmpeg.start(h), "start failed");
      CHECK(poll_until_frame(h, 100), "no frame arrived");
      usleep(100000);
      cb_sleep_us = 300000;
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
      cb_sleep_us = 0;
      before = last_px;
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
      after = last_px;
      moved = (after - before + 256) % 256;
      printf("      %d frame(s) on after a 300 ms callback\n", moved);
      CHECK(moved >= 5, "the thread decoded %d frame(s) during a 300 ms poll", moved);
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }

   printf("   polling a camera that was never started\n");
   current = "poll before start";
   h = open_camera();
   if (h)
   {
      unsigned before = frames_seen;
      /* Refused, not crashed, and no frame invented. */
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb);
      CHECK(frames_seen == before, "a camera that was never started produced a frame");
      camera_ffmpeg.free(h);
   }

   printf("   a source that does not exist\n");
   current = "bad source";
   h = camera_ffmpeg.init("this-is-not-a-filter-graph", CAPS, VIDEO_SCALE_PACK(320, 240));
   if (h)
   {
      /* Taken at init, so start has to be the one that refuses. */
      bool started = camera_ffmpeg.start(h);
      printf("      init took it, start %s\n", started ? "succeeded" : "refused");
      CHECK(!started, "a nonsense source started");
      camera_ffmpeg.free(h);
   }
   else
      printf("      refused at init\n");

   printf("   %u frame(s) through the driver in all\n", frames_seen);
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("camera ffmpeg: opens, restarts, stops mid-frame and frees without leaving a thread behind\n");
   return 0;
}

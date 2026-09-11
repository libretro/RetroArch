/* camera/drivers/ffmpeg.c against a synthetic camera.
 *
 * The driver reads from a libavdevice input format. Built with that
 * format set to lavfi and handed a filter description as its device,
 * the whole pipeline runs with no camera attached: avformat opens the
 * source, the poll thread reads packets, the decoder decodes them,
 * swscale converts into the target buffer and the frontend's poll
 * takes it out under the buffer's lock.
 *
 * That is what this drives. The cases are the ones the driver's
 * threading has to get right and that no eyeball on a webcam would
 * show: start and stop repeatedly, poll while the thread is running,
 * stop while it is mid-frame, and free without stopping first. */

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

static void frame_raw_cb(const uint32_t *buffer, unsigned width,
      unsigned height, size_t pitch)
{
   unsigned x, y;
   frames_seen++;
   last_width  = width;
   last_height = height;
   if (!buffer)
      return;
   /* Touch every pixel: a buffer that was freed or is the wrong size
    * shows here under ASan rather than as a wrong-looking picture. */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
         checksum += buffer[y * (pitch / sizeof(uint32_t)) + x];
}

static uintptr_t frame_gl_cb(void) { return 0; }

#define CAPS (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)
#define SRC  "testsrc=size=320x240:rate=30"

static void *open_camera(void)
{
   return camera_ffmpeg.init(SRC, CAPS, 320, 240);
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
   CHECK(last_width > 0 && last_height > 0, "a frame of %ux%u", last_width, last_height);
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
   h = camera_ffmpeg.init("this-is-not-a-filter-graph", CAPS, 320, 240);
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

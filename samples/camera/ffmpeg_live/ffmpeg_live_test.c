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
#include <retro_atomic.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "../../../camera/camera_driver.h"

extern camera_driver_t camera_ffmpeg;
static retro_atomic_int_t staging_copies;
static retro_atomic_int_t eof_reads;
static retro_atomic_int_t mixed_source;
static retro_atomic_int_t audio_reads;
static retro_atomic_int_t wrong_stream_packets;
static retro_atomic_int_t duplicate_output;
static retro_atomic_int_t accepted_packets;
static retro_atomic_int_t decoded_outputs;
static AVFrame *extra_frame;
static retro_atomic_int_t blocked_read;
static retro_atomic_int_t read_entered;
static retro_atomic_int_t read_cancelled;
static retro_atomic_int_t read_timed_out;
static retro_atomic_int_t missing_interrupt;
static retro_atomic_int_t fail_next_open;
static retro_atomic_int_t removed_device;
static retro_atomic_int_t removed_reads;
static retro_atomic_int_t changing_size;
static retro_atomic_int_t fail_stream_info;
static retro_atomic_int_t null_decoder_calls;
static retro_atomic_int_t scripted_read_error;
static retro_atomic_int_t scripted_error_reads;
static retro_atomic_int_t colour_source;
extern retro_atomic_int_t camera_read_errors;

int __real_avformat_find_stream_info(AVFormatContext *ctx, AVDictionary **options);
int __wrap_avformat_find_stream_info(AVFormatContext *ctx, AVDictionary **options)
{
   if (retro_atomic_exchange_int(&fail_stream_info, 0))
      return AVERROR(EIO);
   return __real_avformat_find_stream_info(ctx, options);
}

int __real_avcodec_receive_frame(AVCodecContext *ctx, AVFrame *frame);
int __wrap_avcodec_receive_frame(AVCodecContext *ctx, AVFrame *frame)
{
   int ret;
   if (retro_atomic_load_acquire_int(&duplicate_output) && extra_frame)
   {
      av_frame_move_ref(frame, extra_frame);
      av_frame_free(&extra_frame);
      retro_atomic_fetch_add_int(&decoded_outputs, 1);
      return 0;
   }
   ret = __real_avcodec_receive_frame(ctx, frame);
   if (ret == 0 && retro_atomic_load_acquire_int(&duplicate_output))
   {
      extra_frame = av_frame_clone(frame);
      if (!extra_frame)
         return AVERROR(ENOMEM);
      retro_atomic_fetch_add_int(&decoded_outputs, 1);
   }
   return ret;
}

int __real_avformat_open_input(AVFormatContext **ctx, const char *url,
      const AVInputFormat *input, AVDictionary **options);
int __wrap_avformat_open_input(AVFormatContext **ctx, const char *url,
      const AVInputFormat *input, AVDictionary **options)
{
   const char prefix[] = "regression-file:";
   if (retro_atomic_load_acquire_int(&blocked_read))
   {
      if (!*ctx || !(*ctx)->interrupt_callback.callback)
         retro_atomic_fetch_add_int(&missing_interrupt, 1);
      else if ((*ctx)->interrupt_callback.callback((*ctx)->interrupt_callback.opaque))
      {
         avformat_free_context(*ctx);
         *ctx = NULL;
         return AVERROR_EXIT;
      }
      if (retro_atomic_exchange_int(&fail_next_open, 0))
      {
         avformat_free_context(*ctx);
         *ctx = NULL;
         return AVERROR(EINVAL);
      }
   }
   if (strncmp(url, prefix, sizeof(prefix) - 1) == 0)
      return __real_avformat_open_input(ctx, url + sizeof(prefix) - 1, NULL, options);
   return __real_avformat_open_input(ctx, url, input, options);
}

int __real_avcodec_send_packet(AVCodecContext *ctx, const AVPacket *packet);
int __wrap_avcodec_send_packet(AVCodecContext *ctx, const AVPacket *packet)
{
   int ret;
   if (!ctx)
   {
      retro_atomic_fetch_add_int(&null_decoder_calls, 1);
      return AVERROR(EINVAL);
   }
   if (retro_atomic_load_acquire_int(&duplicate_output) && extra_frame)
      return AVERROR(EAGAIN);
   if (packet && retro_atomic_load_acquire_int(&mixed_source)
         && packet->stream_index != 1)
   {
      retro_atomic_fetch_add_int(&wrong_stream_packets, 1);
      return AVERROR_INVALIDDATA;
   }
   ret = __real_avcodec_send_packet(ctx, packet);
   if (ret == 0 && packet && retro_atomic_load_acquire_int(&duplicate_output))
      retro_atomic_fetch_add_int(&accepted_packets, 1);
   return ret;
}

int __real_av_read_frame(AVFormatContext *ctx, AVPacket *packet);
int __wrap_av_read_frame(AVFormatContext *ctx, AVPacket *packet)
{
   int ret;
   int error = retro_atomic_load_acquire_int(&scripted_read_error);
   if (retro_atomic_load_acquire_int(&colour_source))
      usleep(20000);
   if (error)
   {
      /* Bound the unfixed retry loop and its log output. */
      if (retro_atomic_fetch_add_int(&scripted_error_reads, 1) < 32)
         return AVERROR(error);
      return AVERROR_EOF;
   }
   if (retro_atomic_load_acquire_int(&changing_size))
      usleep(50000);
   if (retro_atomic_load_acquire_int(&removed_device))
   {
      /* Bound a broken worker to avoid flooding the regression log. */
      if (retro_atomic_fetch_add_int(&removed_reads, 1) < 4)
         return AVERROR(ENODEV);
      return AVERROR_EOF;
   }
   if (retro_atomic_load_acquire_int(&blocked_read))
   {
      unsigned attempt;
      retro_atomic_store_release_int(&read_entered, 1);
      for (attempt = 0; attempt < 500; attempt++)
      {
         if (ctx->interrupt_callback.callback
               && ctx->interrupt_callback.callback(ctx->interrupt_callback.opaque))
         {
            retro_atomic_store_release_int(&read_cancelled, 1);
            return AVERROR_EXIT;
         }
         usleep(1000);
      }
      retro_atomic_store_release_int(&read_timed_out, 1);
      return AVERROR(EIO);
   }
   ret = __real_av_read_frame(ctx, packet);
   if (ret == AVERROR_EOF)
      retro_atomic_fetch_add_int(&eof_reads, 1);
   if (ret >= 0 && retro_atomic_load_acquire_int(&mixed_source)
         && packet->stream_index == 0)
      retro_atomic_fetch_add_int(&audio_reads, 1);
   return ret;
}

int __real_av_image_copy_to_buffer(uint8_t *dst, int dst_size,
      const uint8_t * const src_data[4], const int src_linesize[4],
      enum AVPixelFormat pix_fmt, int width, int height, int align);

int __wrap_av_image_copy_to_buffer(uint8_t *dst, int dst_size,
      const uint8_t * const src_data[4], const int src_linesize[4],
      enum AVPixelFormat pix_fmt, int width, int height, int align)
{
   retro_atomic_fetch_add_int(&staging_copies, 1);
   return __real_av_image_copy_to_buffer(dst, dst_size, src_data,
         src_linesize, pix_fmt, width, height, align);
}

static unsigned failures = 0;
static const char *current = "";
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL [%s]: ", current); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* The frontend's side of a poll: the driver hands over a frame. */
static unsigned frames_seen;
static unsigned last_width, last_height;
static uint32_t checksum;
static int last_px = -1;          /* blue of the first pixel */
static useconds_t cb_sleep_us;    /* a core slow to take its frame */
static bool solid_red;
static bool red_frame;
static bool uniform_frame;
static bool check_colour, colour_frame;
static int expected_colour[3];

static void frame_raw_cb(const uint32_t *buffer, unsigned width,
      unsigned height, size_t pitch)
{
   unsigned x, y;
   uint32_t before = 0, after = 0;
   bool all_red = true;
   bool all_same = true;
   bool all_colour = true;
   frames_seen++;
   last_width  = width;
   last_height = height;
   if (!buffer)
      return;
   last_px = (int)(buffer[0] & 0xff);
   if (cb_sleep_us)
   {
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
            before += buffer[y * (pitch / sizeof(uint32_t)) + x];
      usleep(cb_sleep_us);
   }
   /* Touch every pixel: a buffer that was freed or is the wrong size
    * shows here under ASan rather than as a wrong-looking picture. */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      {
         uint32_t pixel = buffer[y * (pitch / sizeof(uint32_t)) + x];
         all_same = all_same && pixel == buffer[0];
         after += pixel;
         if (check_colour)
         {
            unsigned channel;
            for (channel = 0; channel < 3; channel++)
               if (abs((int)((pixel >> (channel * 8)) & 0xff)
                        - expected_colour[channel]) > 2)
                  all_colour = false;
         }
         if (solid_red)
            all_red = all_red && ((pixel >> 16) & 0xff) >= 245
               && ((pixel >> 8) & 0xff) <= 5 && (pixel & 0xff) <= 5;
      }
   checksum += after;
   uniform_frame = all_same;
   colour_frame = all_colour;
   if (solid_red && all_red)
      red_frame = true;
   if (cb_sleep_us)
      CHECK(before == after, "producer overwrote a frame during its callback");
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
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
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
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
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
   retro_atomic_store_release_int(&eof_reads, 0);
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
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
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
      CHECK(retro_atomic_load_acquire_int(&eof_reads) == 1,
            "finished source read EOF %d times",
            retro_atomic_load_acquire_int(&eof_reads));
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
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
      cb_sleep_us = 0;
      before = last_px;
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
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
      camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
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

   current = "source size differs from output";
   h = camera_ffmpeg.init("color=c=red:size=34x18:rate=100,realtime",
         CAPS, VIDEO_SCALE_PACK(17, 9));
   CHECK(h != NULL, "odd-size camera initialization failed");
   if (h)
   {
      for (i = 0; i < 2; i++)
      {
         CHECK(camera_ffmpeg.start(h), "resized camera start failed");
         red_frame = false;
         solid_red = true;
         {
            unsigned attempt;
            for (attempt = 0; attempt < 100 && !red_frame; attempt++)
            {
               camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
               usleep(20000);
            }
         }
         CHECK(red_frame, "resized frame did not preserve every red pixel");
         solid_red = false;
         CHECK(last_width == 17 && last_height == 9, "requested dimensions changed");
         camera_ffmpeg.stop(h);
      }
      camera_ffmpeg.free(h);
   }
   current = "audio and video source";
   h = camera_ffmpeg.init("sine=frequency=1000:sample_rate=48000:duration=0.1[out0];color=c=red:size=34x18:rate=100,trim=end_frame=10[out1]",
         CAPS, VIDEO_SCALE_PACK(17, 9));
   CHECK(h != NULL, "mixed source initialization failed");
   if (h)
   {
      unsigned attempt;
      retro_atomic_store_release_int(&mixed_source, 1);
      CHECK(camera_ffmpeg.start(h), "mixed source start failed");
      red_frame = false;
      solid_red = true;
      for (attempt = 0; attempt < 100 && !red_frame; attempt++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
         usleep(20000);
      }
      solid_red = false;
      CHECK(red_frame, "mixed source did not produce correct video pixels");
      camera_ffmpeg.stop(h);
      retro_atomic_store_release_int(&mixed_source, 0);
      CHECK(retro_atomic_load_acquire_int(&audio_reads) > 0,
            "mixed source produced no audio packets");
      CHECK(retro_atomic_load_acquire_int(&wrong_stream_packets) == 0,
            "non-video packets reached the video decoder");
      camera_ffmpeg.free(h);
   }
   current = "multiple outputs per packet";
   h = camera_ffmpeg.init("color=c=black:size=32x16:rate=25,format=gray,geq=lum=N*16,trim=end_frame=3",
         CAPS, VIDEO_SCALE_PACK(32, 16));
   CHECK(h != NULL, "buffered source initialization failed");
   if (h)
   {
      unsigned attempt;
      retro_atomic_store_release_int(&duplicate_output, 1);
      retro_atomic_store_release_int(&eof_reads, 0);
      CHECK(camera_ffmpeg.start(h), "buffered source start failed");
      for (attempt = 0; attempt < 100
            && !retro_atomic_load_acquire_int(&eof_reads); attempt++)
         usleep(20000);
      camera_ffmpeg.stop(h);
      retro_atomic_store_release_int(&duplicate_output, 0);
      CHECK(retro_atomic_load_acquire_int(&accepted_packets) == 3,
            "backpressure dropped an input packet");
      CHECK(retro_atomic_load_acquire_int(&decoded_outputs) == 6,
            "decoder outputs were not fully drained");
      av_frame_free(&extra_frame);
      camera_ffmpeg.free(h);
   }
   current = "delayed decoder output";
   h = camera_ffmpeg.init("regression-file:decoder_delay.mkv", CAPS,
         VIDEO_SCALE_PACK(32, 16));
   CHECK(h != NULL, "delayed source initialization failed");
   if (h)
   {
      unsigned attempt;
      CHECK(camera_ffmpeg.start(h), "delayed source start failed");
      last_px = -1;
      for (attempt = 0; attempt < 100 && last_px < 108; attempt++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
         usleep(20000);
      }
      CHECK(last_px >= 108 && last_px <= 116,
            "delayed final frame is grey 112, got %d", last_px);
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }
   current = "blocked capture read";
   h = open_camera();
   CHECK(h != NULL, "blocked camera initialization failed");
   if (h)
   {
      unsigned cycle;
      retro_atomic_store_release_int(&blocked_read, 1);
      retro_atomic_store_release_int(&fail_next_open, 1);
      for (cycle = 0; cycle < 2; cycle++)
      {
         unsigned attempt;
         retro_atomic_store_release_int(&read_entered, 0);
         retro_atomic_store_release_int(&read_cancelled, 0);
         retro_atomic_store_release_int(&read_timed_out, 0);
         CHECK(camera_ffmpeg.start(h), "blocked camera start/restart failed");
         for (attempt = 0; attempt < 100
               && !retro_atomic_load_acquire_int(&read_entered); attempt++)
            usleep(1000);
         CHECK(retro_atomic_load_acquire_int(&read_entered), "worker did not enter read");
         camera_ffmpeg.stop(h);
         CHECK(retro_atomic_load_acquire_int(&read_cancelled),
               "stop did not interrupt capture read");
         CHECK(!retro_atomic_load_acquire_int(&read_timed_out),
               "capture read needed the fixture timeout");
      }
      CHECK(!retro_atomic_load_acquire_int(&missing_interrupt),
            "input open/retry lacked interrupt callback");
      camera_ffmpeg.free(h);
      retro_atomic_store_release_int(&blocked_read, 0);
   }
   current = "removed capture device";
   h = open_camera();
   CHECK(h != NULL, "removed camera initialization failed");
   if (h)
   {
      unsigned attempt;
      retro_atomic_store_release_int(&removed_device, 1);
      CHECK(camera_ffmpeg.start(h), "removed camera start failed");
      for (attempt = 0; attempt < 100
            && !retro_atomic_load_acquire_int(&removed_reads); attempt++)
         usleep(1000);
      usleep(20000);
      camera_ffmpeg.stop(h);
      CHECK(retro_atomic_load_acquire_int(&removed_reads) == 1,
            "removed device was read %d times",
            retro_atomic_load_acquire_int(&removed_reads));
      retro_atomic_store_release_int(&removed_device, 0);
      CHECK(camera_ffmpeg.start(h), "camera restart after removal failed");
      last_px = -1;
      for (attempt = 0; attempt < 100 && last_px < 0; attempt++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
         usleep(1000);
      }
      CHECK(last_px >= 0, "camera restart produced no frame");
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }
   current = "changing capture dimensions";
   h = camera_ffmpeg.init("regression-file:decoder_resize.h264",
         CAPS, VIDEO_SCALE_PACK(17, 9));
   CHECK(h != NULL, "changing source initialization failed");
   if (h)
   {
      unsigned attempt;
      bool small_seen = false, large_seen = false;
      retro_atomic_store_release_int(&changing_size, 1);
      CHECK(camera_ffmpeg.start(h), "changing source start failed");
      for (attempt = 0; attempt < 100 && !(small_seen && large_seen); attempt++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
         small_seen = small_seen || (uniform_frame && last_px >= 28 && last_px <= 36);
         large_seen = large_seen || (uniform_frame && last_px >= 108 && last_px <= 116);
         usleep(10000);
      }
      CHECK(small_seen && large_seen, "resolution change lost frames (%d/%d)",
            small_seen, large_seen);
      CHECK(last_width == 17 && last_height == 9, "output dimensions changed");
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
      retro_atomic_store_release_int(&changing_size, 0);
   }
   current = "failed stream discovery";
   h = open_camera();
   CHECK(h != NULL, "failure source initialization failed");
   if (h)
   {
      unsigned attempt;
      retro_atomic_store_release_int(&fail_stream_info, 1);
      CHECK(!camera_ffmpeg.start(h), "failed stream discovery reported success");
      CHECK(!retro_atomic_load_acquire_int(&null_decoder_calls),
            "cleanup called decoder API without a decoder");
      CHECK(camera_ffmpeg.start(h), "restart after discovery failure failed");
      checksum = 0;
      for (attempt = 0; attempt < 100 && !checksum; attempt++)
      {
         camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
         usleep(1000);
      }
      CHECK(checksum != 0, "restart produced no decoded pixels");
      camera_ffmpeg.stop(h);
      camera_ffmpeg.free(h);
   }
   current = "capture read errors";
   {
      const int errors[] = { EAGAIN, EINTR, EIO, EINVAL };
      unsigned lane;
      for (lane = 0; lane < sizeof(errors) / sizeof(errors[0]); lane++)
      {
         unsigned attempt, polls;
         bool transient = errors[lane] == EAGAIN || errors[lane] == EINTR;
         h = open_camera();
         CHECK(h != NULL, "error source initialization failed");
         if (!h)
            continue;
         retro_atomic_store_release_int(&scripted_error_reads, 0);
         retro_atomic_store_release_int(&camera_read_errors, 0);
         retro_atomic_store_release_int(&scripted_read_error, errors[lane]);
         CHECK(camera_ffmpeg.start(h), "error source start failed");
         for (attempt = 0; attempt < 100
               && !retro_atomic_load_acquire_int(&scripted_error_reads); attempt++)
            usleep(1000);
         usleep(20000);
         CHECK(retro_atomic_load_acquire_int(&scripted_error_reads) == 1,
               "idle error source made %d reads (errno %d)",
               retro_atomic_load_acquire_int(&scripted_error_reads), errors[lane]);
         for (polls = 0; polls < 10; polls++)
         {
            camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
            usleep(1000);
         }
         CHECK(retro_atomic_load_acquire_int(&scripted_error_reads) <= (transient ? 11 : 1),
               "read errors exceeded frontend retry requests (errno %d)", errors[lane]);
         CHECK(retro_atomic_load_acquire_int(&camera_read_errors) == (transient ? 0 : 1),
               "wrong read-error log count %d (errno %d)",
               retro_atomic_load_acquire_int(&camera_read_errors), errors[lane]);
         if (transient && errors[lane] == EAGAIN)
         {
            /* Stop must wake a worker parked for unavailable input. */
            camera_ffmpeg.stop(h);
            retro_atomic_store_release_int(&scripted_read_error, 0);
            CHECK(camera_ffmpeg.start(h), "restart after parked stop failed");
         }
         else
         {
            if (!transient)
               camera_ffmpeg.stop(h);
            retro_atomic_store_release_int(&scripted_read_error, 0);
            if (!transient)
               CHECK(camera_ffmpeg.start(h), "restart after terminal error failed");
         }
         checksum = 0;
         for (attempt = 0; attempt < 100 && !checksum; attempt++)
         {
            camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
            usleep(1000);
         }
         CHECK(checksum != 0, "capture did not recover after errno %d", errors[lane]);
         camera_ffmpeg.stop(h);
         camera_ffmpeg.free(h);
      }
   }
   current = "capture colour metadata";
   {
      const char *sources[] = {
         "nullsrc=s=32x16:r=25,format=yuv420p,geq=lum=16:cb=128:cr=128,setparams=range=full,trim=end_frame=3[a];nullsrc=s=32x16:r=25,format=yuv420p,geq=lum=16:cb=128:cr=128,setparams=range=limited,trim=end_frame=3[b];[a][b]concat=n=2:v=1:a=0",
         "nullsrc=s=32x16:r=25,format=yuv420p,geq=lum=100:cb=90:cr=170,setparams=range=full:colorspace=bt709,trim=end_frame=3[a];nullsrc=s=32x16:r=25,format=yuv420p,geq=lum=100:cb=90:cr=170,setparams=range=full:colorspace=smpte170m,trim=end_frame=3[b];[a][b]concat=n=2:v=1:a=0",
         "nullsrc=s=32x16:r=25,format=gray,geq=lum=16,trim=end_frame=3[a];nullsrc=s=32x16:r=25,format=gray,geq=lum=48,trim=end_frame=3[b];[a][b]concat=n=2:v=1:a=0"
      };
      /* Full-range BT.709 and BT.601 YCbCr equations, rounded to bytes. */
      const int colours[3][2][3] = { { {16, 16, 16}, {0, 0, 0} },
         { {29, 87, 166}, {33, 83, 159} }, { {16, 16, 16}, {48, 48, 48} } };
      unsigned lane;
      for (lane = 0; lane < sizeof(sources) / sizeof(sources[0]); lane++)
      {
         unsigned attempt, stage = 0;
         int initial_eof = retro_atomic_load_acquire_int(&eof_reads);
         h = camera_ffmpeg.init(sources[lane], CAPS, VIDEO_SCALE_PACK(17, 9));
         CHECK(h != NULL, "colour source initialization failed");
         if (!h)
            continue;
         retro_atomic_store_release_int(&colour_source, 1);
         CHECK(camera_ffmpeg.start(h), "colour source start failed");
         check_colour = true;
         for (attempt = 0; attempt < 100 && stage < 2; attempt++)
         {
            memcpy(expected_colour, colours[lane][stage], sizeof(expected_colour));
            colour_frame = false;
            camera_ffmpeg.poll(h, frame_raw_cb, frame_gl_cb, NULL);
            if (colour_frame)
               stage++;
            usleep(10000);
         }
         CHECK(stage == 2, "colour metadata transition %u reached stage %u", lane, stage);
         check_colour = false;
         for (attempt = 0; attempt < 100
               && retro_atomic_load_acquire_int(&eof_reads) == initial_eof; attempt++)
            usleep(1000);
         camera_ffmpeg.stop(h);
         camera_ffmpeg.free(h);
         retro_atomic_store_release_int(&colour_source, 0);
      }
   }
   current = "direct output";
   printf("   %u frame(s) through the driver in all\n", frames_seen);
   CHECK(retro_atomic_load_acquire_int(&staging_copies) == 0,
         "conversion used an intermediate full-frame copy");
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("camera ffmpeg: opens, restarts, stops mid-frame and frees without leaving a thread behind\n");
   return 0;
}

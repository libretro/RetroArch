#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <formats/rwebm_video.h>
#include <formats/image.h>
#include <formats/rh265.h>
#include <retro_timers.h>
#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #x); goto done; } } while (0)

static const uint32_t *next_frame(rwebm_video_stream_t *stream, int *duration)
{
   unsigned polls;
   for (polls = 0; polls < 10000; polls++)
   {
      const uint32_t *frame = rwebm_video_stream_next(stream, duration);
      if (frame || !rwebm_video_stream_waiting(stream)) return frame;
      retro_sleep(1);
   }
   return NULL;
}

int main(int argc, char **argv)
{
   FILE *file = NULL, *reference = NULL, *times = NULL;
   long length;
   uint8_t *data = NULL;
   uint8_t *pixels = NULL;
   const uint32_t *frame;
   uint32_t *output = NULL;
   rwebm_video_stream_t *stream = NULL;
   void *pool = NULL;
   const char *workers_env = getenv("H26X_TEST_THREADS");
   unsigned workers = workers_env ? (unsigned)atoi(workers_env) : 0;
   unsigned pass, count, width, height;
   unsigned expected[24], target[5];
   int duration, total, endpoint, status = 1;
   int64_t timestamp;
   CHECK(argc >= 2 && argc <= 4);
   CHECK(workers <= 8);
#ifdef HAVE_THREADS
   if (workers)
   {
      rh265_video_set_publish_delay(200);
      pool = tpool_create(workers);
      CHECK(pool);
   }
#else
   CHECK(!workers);
#endif
   for (count = 0; count < 24; count++)
      expected[count] = (count * 1000 + 12) / 24;
   if (argc == 4)
   {
      times = fopen(argv[3], "r");
      CHECK(times);
      for (count = 0; count < 24; count++)
      {
         unsigned long value;
         CHECK(fscanf(times, "%lu", &value) == 1 && value < 2147483647UL);
         expected[count] = (unsigned)value;
      }
      fclose(times);
      times = NULL;
   }
   endpoint = (int)(expected[23] + expected[23] - expected[22]);
   target[0] = expected[12]; target[1] = expected[3];
   target[2] = expected[23]; target[3] = expected[23] + 1000;
   target[4] = expected[0];
   if (argc >= 3)
   {
      reference = fopen(argv[2], "rb");
      pixels = (uint8_t*)malloc(64 * 64 * 4);
      CHECK(reference && pixels);
   }
   file = fopen(argv[1], "rb");
   CHECK(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   CHECK(length > 0 && !fseek(file, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)length);
   CHECK(data && fread(data, 1, (size_t)length, file) == (size_t)length);
   fclose(file);
   file = NULL;
   stream = rwebm_video_stream_open(data, (size_t)length);
   CHECK(stream);
   rwebm_video_stream_set_blit_pool(stream, pool, workers);
   CHECK((rwebm_video_stream_h264(stream) != NULL)
         != (rwebm_video_stream_h265(stream) != NULL));
   CHECK(image_transfer_anim_stream_h264(stream, IMAGE_TYPE_WEBM)
         == rwebm_video_stream_h264(stream));
   CHECK(image_transfer_anim_stream_h265(stream, IMAGE_TYPE_WEBM)
         == rwebm_video_stream_h265(stream));
   CHECK(!rwebm_video_stream_set_hw(stream, 0));
   CHECK(!rwebm_video_stream_set_hw(stream, 1));
   rwebm_video_stream_get_info(stream, &width, &height, NULL, NULL);
   CHECK(width == 64 && height == 64);
   output = (uint32_t*)malloc(64 * 64 * sizeof(*output));
   CHECK(output);
   for (pass = 0; pass < 2; pass++)
   {
      count = 0;
      total = 0;
      timestamp = -1;
      while ((frame = next_frame(stream, &duration)) != NULL)
      {
         int64_t next = rwebm_video_stream_frame_time_ns(stream);
         CHECK(count < 24);
         CHECK(next > timestamp && duration > 0);
         CHECK(next == (int64_t)expected[count] * 1000000);
         if (reference)
         {
            const uint8_t *actual = (const uint8_t*)frame;
            unsigned i, sum = 0, peak = 0;
            CHECK(fread(pixels, 1, 64 * 64 * 4, reference) == 64 * 64 * 4);
            for (i = 0; i < 64 * 64 * 4; i++)
            {
               unsigned delta = actual[i] > pixels[i]
                  ? actual[i] - pixels[i] : pixels[i] - actual[i];
               sum += delta;
               if (delta > peak) peak = delta;
            }
            if (sum >= 64 * 64 * 4 * 4 || peak > 80)
               fprintf(stderr, "frame %u: error sum=%u peak=%u RGB=%u,%u,%u expected=%u,%u,%u\n",
                     count, sum, peak, actual[0], actual[1], actual[2],
                     pixels[0], pixels[1], pixels[2]);
            CHECK(sum < 64 * 64 * 4 * 4 && peak <= 80);
         }
         timestamp = next;
         total += duration;
         count++;
         CHECK(count <= 24);
      }
      CHECK(count == 24 && rwebm_video_stream_completed(stream));
      if (total < endpoint - 1 || total > endpoint + 1)
         fprintf(stderr, "display duration: %d ms\n", total);
      CHECK(total >= endpoint - 1 && total <= endpoint + 1);
      if (reference) CHECK(!fseek(reference, 0, SEEK_SET));
      rwebm_video_stream_rewind(stream);
   }
   for (pass = 0; pass < 5; pass++)
   {
      int result = 0;
      int64_t position = -1;
      unsigned steps = 0;
      CHECK(rwebm_video_stream_seek_begin_ns(stream,
               (int64_t)target[pass] * 1000000));
      while (!result && steps++ < 100)
      {
         result = rwebm_video_stream_seek_step(stream, &position);
         if (!result) retro_sleep(1);
      }
      CHECK(result == 1 && steps < 100);
      memset(output, 0, 64 * 64 * sizeof(*output));
      rwebm_video_stream_set_output(stream, output);
      frame = next_frame(stream, &duration);
      CHECK(frame == output && output[0]);
      timestamp = rwebm_video_stream_frame_time_ns(stream);
      CHECK(timestamp == (int64_t)(target[pass] > expected[23] ? expected[23] : target[pass])
            * 1000000 && position == timestamp / 1000);
      if (reference)
      {
         const uint8_t *actual = (const uint8_t*)frame;
         unsigned i, sum = 0, peak = 0;
         unsigned index = 0;
         long offset;
         while (index < 24 && (int64_t)expected[index] * 1000000 != timestamp)
            index++;
         CHECK(index < 24);
         offset = (long)index * 64 * 64 * 4;
         CHECK(!fseek(reference, offset, SEEK_SET));
         CHECK(fread(pixels, 1, 64 * 64 * 4, reference) == 64 * 64 * 4);
         for (i = 0; i < 64 * 64 * 4; i++)
         {
            unsigned delta = actual[i] > pixels[i]
               ? actual[i] - pixels[i] : pixels[i] - actual[i];
            sum += delta;
            if (delta > peak) peak = delta;
         }
         CHECK(sum < 64 * 64 * 4 * 4 && peak <= 80);
      }
      if (timestamp < (int64_t)expected[23] * 1000000)
      {
         CHECK(next_frame(stream, &duration));
         CHECK(rwebm_video_stream_frame_time_ns(stream) > timestamp);
      }
   }
   rwebm_video_stream_close(stream);
   stream = NULL;
   for (pass = 0; pass < 3; pass++)
   {
      size_t avail = 0;
      unsigned polls = 0;
      int need_more;
      stream = NULL;
      count = 0;
      total = 0;
      while (!stream && avail < (size_t)length)
      {
         avail += (size_t)length - avail < 64 ? (size_t)length - avail : 64;
         stream = rwebm_video_stream_open_avail(data, (size_t)length,
               avail, &need_more);
         CHECK(stream || need_more);
      }
      CHECK(stream);
      rwebm_video_stream_set_blit_pool(stream, pool, workers);
      while (polls++ < 2000)
      {
         frame = rwebm_video_stream_next(stream, &duration);
         if (frame)
         {
            CHECK(count < 24);
            CHECK(rwebm_video_stream_frame_time_ns(stream)
                  == (int64_t)expected[count] * 1000000);
            if (reference)
            {
               const uint8_t *actual = (const uint8_t*)frame;
               unsigned i, sum = 0, peak = 0;
               CHECK(!fseek(reference, (long)count * 64 * 64 * 4, SEEK_SET));
               CHECK(fread(pixels, 1, 64 * 64 * 4, reference) == 64 * 64 * 4);
               for (i = 0; i < 64 * 64 * 4; i++)
               {
                  unsigned delta = actual[i] > pixels[i]
                     ? actual[i] - pixels[i] : pixels[i] - actual[i];
                  sum += delta;
                  if (delta > peak) peak = delta;
               }
               CHECK(sum < 64 * 64 * 4 * 4 && peak <= 80);
            }
            total += duration;
            count++;
            if (!pass && count == 8) break;
            if (pass == 2 && count == 8)
            {
               int frames;
               avail = (size_t)length;
               rwebm_video_stream_set_avail(stream, avail);
               rwebm_video_stream_complete_scan(stream, data, avail);
               rwebm_video_stream_get_info(stream, NULL, NULL, &frames, NULL);
               CHECK(frames == 24);
            }
         }
         else if (rwebm_video_stream_completed(stream))
            break;
         else
         {
            size_t floor, need;
            CHECK(rwebm_video_stream_waiting(stream));
            rwebm_video_stream_read_span(stream, &floor, &need);
            if (need > avail || (!need && avail < (size_t)length))
            {
               CHECK(avail < (size_t)length);
               avail += (size_t)length - avail < 64 ? (size_t)length - avail : 64;
               rwebm_video_stream_set_avail(stream, avail);
            }
            else
               retro_sleep(1);
         }
      }
      if (polls >= 2000 || count != (pass ? 24 : 8))
      {
         size_t lo, hi, pin_lo, pin_hi;
         rwebm_video_stream_read_span(stream, &lo, &hi);
         rwebm_video_stream_pin_span(stream, &pin_lo, &pin_hi);
         fprintf(stderr, "partial pass=%u count=%u polls=%u avail=%lu/%lu need=%lu..%lu pin=%lu..%lu waiting=%d completed=%d\n",
               pass, count, polls, (unsigned long)avail, (unsigned long)length,
               (unsigned long)lo, (unsigned long)hi, (unsigned long)pin_lo,
               (unsigned long)pin_hi, rwebm_video_stream_waiting(stream),
               rwebm_video_stream_completed(stream));
      }
      CHECK(polls < 2000 && count == (pass ? 24 : 8));
      if (pass)
         CHECK(rwebm_video_stream_completed(stream)
               && total >= endpoint - 1 && total <= endpoint + 1);
      rwebm_video_stream_close(stream);
      stream = NULL;
   }
   status = 0;
done:
   if (file) fclose(file);
   if (times) fclose(times);
   if (stream) rwebm_video_stream_close(stream);
#ifdef HAVE_THREADS
   if (pool) tpool_destroy(pool);
#endif
   free(output);
   free(data);
   free(pixels);
   if (reference) fclose(reference);
   if (!status) puts("Matroska H26x display order, drain and rewind passed");
   return status;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <formats/rmp4_video.h>
#include <formats/image.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #x); goto done; } } while (0)

int main(int argc, char **argv)
{
   FILE *file = NULL, *reference = NULL, *times = NULL;
   long length;
   uint8_t *data = NULL;
   uint8_t *pixels = NULL;
   const uint32_t *frame;
   uint32_t *output = NULL;
   rmp4_video_stream_t *stream = NULL;
   unsigned pass, count, width, height;
   unsigned expected[24], target[5];
   int duration, total, endpoint, status = 1;
   int64_t timestamp;
   CHECK(argc >= 2 && argc <= 4);
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
   stream = rmp4_video_stream_open(data, (size_t)length);
   CHECK(stream);
   CHECK((rmp4_video_stream_h264(stream) != NULL)
         != (rmp4_video_stream_h265(stream) != NULL));
   CHECK(!rmp4_video_stream_set_hw(stream, 0));
   CHECK(!rmp4_video_stream_set_hw(stream, 1));
   rmp4_video_stream_get_info(stream, &width, &height, NULL, NULL);
   CHECK(width == 64 && height == 64);
   output = (uint32_t*)malloc(64 * 64 * sizeof(*output));
   CHECK(output);
   for (pass = 0; pass < 2; pass++)
   {
      count = 0;
      total = 0;
      timestamp = -1;
      while ((frame = rmp4_video_stream_next(stream, &duration)) != NULL)
      {
         int64_t next = rmp4_video_stream_frame_time_ns(stream);
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
      CHECK(count == 24 && rmp4_video_stream_completed(stream));
      if (total < endpoint - 1 || total > endpoint + 1)
         fprintf(stderr, "display duration: %d ms\n", total);
      CHECK(total >= endpoint - 1 && total <= endpoint + 1);
      if (reference) CHECK(!fseek(reference, 0, SEEK_SET));
      rmp4_video_stream_rewind(stream);
   }
   for (pass = 0; pass < 5; pass++)
   {
      int result = 0;
      int64_t position = -1;
      unsigned steps = 0;
      CHECK(rmp4_video_stream_seek_movie_begin_ns(stream,
               (int64_t)target[pass] * 1000000));
      while (!result && steps++ < 100)
         result = rmp4_video_stream_seek_movie_step(stream, &position);
      CHECK(result == 1 && steps < 100);
      memset(output, 0, 64 * 64 * sizeof(*output));
      rmp4_video_stream_set_output(stream, output);
      frame = rmp4_video_stream_next(stream, &duration);
      CHECK(frame == output && output[0]);
      timestamp = rmp4_video_stream_frame_time_ns(stream);
      if (timestamp / 1000000 != (int64_t)(target[pass] > expected[23]
               ? expected[23] : target[pass]) || position != (int64_t)target[pass] * 1000)
         fprintf(stderr, "seek %u: target=%u actual=%.3f position=%.3f\n",
               pass, target[pass], (double)timestamp / 1000000, (double)position / 1000);
      CHECK(timestamp == (int64_t)(target[pass] > expected[23] ? expected[23] : target[pass])
            * 1000000 && position == (int64_t)target[pass] * 1000);
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
         CHECK(rmp4_video_stream_next(stream, &duration));
         CHECK(rmp4_video_stream_frame_time_ns(stream) > timestamp);
      }
   }
   rmp4_video_stream_close(stream);
   stream = NULL;
   for (pass = 0; pass < 2; pass++)
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
         stream = rmp4_video_stream_open_avail(data, (size_t)length,
               avail, &need_more, NULL, NULL);
         CHECK(stream || need_more);
      }
      CHECK(stream);
      while (polls++ < 2000)
      {
         frame = rmp4_video_stream_next(stream, &duration);
         if (frame)
         {
            CHECK(count < 24);
            CHECK(rmp4_video_stream_frame_time_ns(stream)
                  == (int64_t)expected[count] * 1000000);
            total += duration;
            count++;
            if (!pass && count == 8) break;
         }
         else if (rmp4_video_stream_completed(stream))
            break;
         else
         {
            CHECK(rmp4_video_stream_waiting(stream));
            avail += (size_t)length - avail < 64 ? (size_t)length - avail : 64;
            rmp4_video_stream_set_avail(stream, avail);
         }
      }
      CHECK(polls < 2000 && count == (pass ? 24 : 8));
      if (pass)
         CHECK(rmp4_video_stream_completed(stream)
               && total >= endpoint - 1 && total <= endpoint + 1);
      rmp4_video_stream_close(stream);
      stream = NULL;
   }
   status = 0;
done:
   if (file) fclose(file);
   if (times) fclose(times);
   if (stream) rmp4_video_stream_close(stream);
   free(output);
   free(data);
   free(pixels);
   if (reference) fclose(reference);
   if (!status) puts("MP4 H26x display order, drain and rewind passed");
   return status;
}

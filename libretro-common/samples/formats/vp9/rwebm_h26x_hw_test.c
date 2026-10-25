#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rh264.h>
#include <formats/rh265_hw.h>
#include <formats/rwebm_video.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #x); return 1; } } while (0)

static struct
{
   unsigned width, height, slots, opened, closed, acquired, released;
   unsigned errors;
   unsigned char live[4096];
   int poc[32], decode_busy, output_busy;
} fake;

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)owner; (void)done;
   if (!value || value >= sizeof(fake.live) || !fake.live[(unsigned)value])
   {
      fake.errors++;
      return;
   }
   fake.live[(unsigned)value] = 0;
   fake.released++;
}

static int output(void *decoder, unsigned slot, video_hw_frame_t *frame)
{
   (void)decoder;
   if (!fake.output_busy) { fake.output_busy = 1; return 1; }
   fake.output_busy = 0;
   if (slot >= fake.slots) return -1;
   memset(frame, 0, sizeof(*frame));
   frame->image = (uint64_t)(fake.poc[slot] + 1);
   frame->width = fake.width;
   frame->height = fake.height;
   frame->api = VIDEO_HW_API_VULKAN;
   if (++fake.acquired >= sizeof(fake.live)) return -1;
   frame->done_value = fake.acquired;
   fake.live[fake.acquired] = 1;
   frame->abandon = abandon;
   return 0;
}

static void *open264(void *user, const rh264_hw_sps_t *sps, unsigned slots)
{
   (void)user;
   if (slots > 32) return NULL;
   fake.width = sps->width; fake.height = sps->height; fake.slots = slots;
   fake.decode_busy = fake.output_busy = 0;
   fake.opened++;
   return &fake;
}

static int decode264(void *decoder, const rh264_hw_pic_t *picture)
{
   (void)decoder;
   if (!fake.decode_busy) { fake.decode_busy = 1; return 1; }
   fake.decode_busy = 0;
   if (picture->slot < 0 || (unsigned)picture->slot >= fake.slots) return -1;
   fake.poc[picture->slot] = picture->poc[0];
   return 0;
}

static void close264(void *decoder)
{
   (void)decoder;
   fake.closed++;
}

static int decode265(void *decoder, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_nal_t *slices,
      const rh265_hw_picture_t *picture)
{
   (void)decoder; (void)params; (void)au; (void)slices;
   if (!fake.decode_busy) { fake.decode_busy = 1; return 1; }
   fake.decode_busy = 0;
   if (picture->slot >= fake.slots) return -1;
   fake.poc[picture->slot] = picture->poc;
   return 0;
}

static bool open265(void *user, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   (void)user; (void)source;
   if (slots > 32) return false;
   fake.width = params->width; fake.height = params->height; fake.slots = slots;
   fake.decode_busy = fake.output_busy = 0;
   fake.opened++;
   ops->decoder = &fake;
   ops->decode = decode265;
   ops->output = output;
   return true;
}

static void close265(rh265_hw_ops_t *ops)
{
   (void)ops;
   fake.closed++;
}

static const rh264_hw_ops_t backend264 = {
   NULL, open264, decode264, output, close264, NULL, VIDEO_HW_API_VULKAN
};
static const rh265_hw_backend_t backend265 = {NULL, open265, close265};

int main(int argc, char **argv)
{
   FILE *file;
   long length;
   uint8_t *data;
   rwebm_video_stream_t *stream;
   video_hw_frame_t frame, duplicate;
   unsigned pass, count, polls;
   int duration, result;
   int64_t position, expected[24];
   CHECK(argc == 2 || argc == 3);
   for (count = 0; count < 24; count++)
      expected[count] = (int64_t)((count * 1000 + 12) / 24) * 1000000;
   if (argc == 3)
   {
      unsigned long ms;
      file = fopen(argv[2], "r");
      CHECK(file);
      for (count = 0; count < 24; count++)
      {
         CHECK(fscanf(file, "%lu", &ms) == 1);
         expected[count] = (int64_t)ms * 1000000;
      }
      fclose(file);
   }
   file = fopen(argv[1], "rb");
   CHECK(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   CHECK(length > 0 && !fseek(file, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)length);
   CHECK(data && fread(data, 1, (size_t)length, file) == (size_t)length);
   fclose(file);
   rh264_set_hw_backend(&backend264);
   rh265_set_hw_backend(&backend265);
   stream = rwebm_video_stream_open(data, (size_t)length);
   CHECK(stream && rwebm_video_stream_set_hw(stream, 1));
   for (pass = 0; pass < 2; pass++)
   {
      count = polls = 0;
      while (polls++ < 1000)
      {
         if (pass)
            result = rwebm_video_stream_skip(stream, &duration);
         else
            result = rwebm_video_stream_next(stream, &duration) ? 1 : 0;
         if (result == 1)
         {
            CHECK(count < 24 && duration > 0);
            CHECK(rwebm_video_stream_frame_time_ns(stream)
                  == expected[count]);
            if (!pass)
            {
               CHECK(rwebm_video_stream_hw_frame(stream, &frame));
               CHECK(frame.width == 64 && frame.height == 64 && frame.image);
               CHECK(!rwebm_video_stream_hw_frame(stream, &duplicate));
               video_hw_frame_abandon(&frame);
            }
            else
               CHECK(!rwebm_video_stream_hw_frame(stream, &frame));
            count++;
         }
         else if (rwebm_video_stream_completed(stream)) break;
         else
         {
            CHECK(rwebm_video_stream_waiting(stream));
            CHECK(!rwebm_video_stream_hw_frame(stream, &duplicate));
         }
      }
      CHECK(polls < 1000 && count == 24 && rwebm_video_stream_completed(stream));
      rwebm_video_stream_rewind(stream);
   }
   CHECK(rwebm_video_stream_seek_begin_ns(stream, expected[12]));
   polls = 0; result = 0;
   while (!result && polls++ < 1000)
      result = rwebm_video_stream_seek_step(stream, &position);
   CHECK(result == 1 && position == expected[12] / 1000);
   CHECK(rwebm_video_stream_next(stream, &duration));
   CHECK(rwebm_video_stream_hw_frame(stream, &frame));
   rwebm_video_stream_close(stream);
   video_hw_frame_abandon(&frame);
   CHECK(!fake.errors && fake.opened == fake.closed && fake.acquired == fake.released);
   stream = rwebm_video_stream_open(data, (size_t)length);
   CHECK(stream && rwebm_video_stream_set_hw(stream, 1));
   CHECK(!rwebm_video_stream_next(stream, &duration));
   rwebm_video_stream_close(stream);
   CHECK(!fake.errors && fake.opened == fake.closed && fake.acquired == fake.released);
   {
      size_t avail = 0;
      int need_more;
      stream = NULL;
      while (!stream && avail < (size_t)length)
      {
         avail += (size_t)length - avail < 64 ? (size_t)length - avail : 64;
         stream = rwebm_video_stream_open_avail(data, (size_t)length,
               avail, &need_more);
         CHECK(stream || need_more);
      }
      CHECK(stream && rwebm_video_stream_set_hw(stream, 1));
      polls = count = 0;
      while (polls++ < 2000)
      {
         if (rwebm_video_stream_next(stream, &duration))
         {
            CHECK(count < 24 && rwebm_video_stream_hw_frame(stream, &frame));
            CHECK(rwebm_video_stream_frame_time_ns(stream)
                  == expected[count]);
            video_hw_frame_abandon(&frame);
            count++;
         }
         else if (rwebm_video_stream_completed(stream)) break;
         else
         {
            CHECK(rwebm_video_stream_waiting(stream));
            CHECK(!rwebm_video_stream_hw_frame(stream, &duplicate));
            avail += (size_t)length - avail < 64 ? (size_t)length - avail : 64;
            rwebm_video_stream_set_avail(stream, avail);
         }
      }
      CHECK(polls < 2000 && count == 24);
      rwebm_video_stream_close(stream);
   }
   CHECK(!fake.errors && fake.opened == fake.closed && fake.acquired == fake.released);
   rh264_set_hw_backend(NULL);
   rh265_set_hw_backend(NULL);
   free(data);
   puts("Matroska hardware retries, leases, skip, seek and cancellation passed");
   return 0;
}

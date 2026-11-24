/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_planar_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A video stream's frames handed out as their decoded planes
 * (image_transfer_anim_stream_set_planar) against the same stream's
 * RGB blit: the planes, converted with the colour flags the stream
 * reports for them, give the picture the blit gives, frame by frame.
 * A stream the container says is not 8-bit 4:2:0 SDR refuses planar
 * output before any decode.
 *
 * Usage: video_planar_test FILE mp4|webm EXPECT [FLAGS]
 *   EXPECT  1 when the stream must take planar output, 0 when it must
 *           refuse it
 *   FLAGS   the IMAGE_YUV_FLAG_* every frame must report */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>

#define TOLERANCE 4

static uint8_t *read_file(const char *path, size_t *len)
{
   uint8_t *data = NULL;
   long size;
   FILE *file = fopen(path, "rb");
   if (!file)
      return NULL;
   if (     !fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0
         && !fseek(file, 0, SEEK_SET)
         && (data = (uint8_t*)malloc((size_t)size))
         && fread(data, 1, (size_t)size, file) != (size_t)size)
   {
      free(data);
      data = NULL;
   }
   fclose(file);
   *len = data ? (size_t)size : 0;
   return data;
}

static int channel_diff(uint32_t a, uint32_t b, unsigned shift)
{
   int d = (int)((a >> shift) & 0xFF) - (int)((b >> shift) & 0xFF);
   return d < 0 ? -d : d;
}

static uint32_t planes_hash(const struct texture_planar *p, unsigned w, unsigned h)
{
   uint32_t hash = 2166136261u;
   unsigned plane, x, y;
   for (plane = 0; plane < 3; plane++)
   {
      unsigned width = plane ? (w + 1) / 2 : w;
      unsigned height = plane ? (h + 1) / 2 : h;
      unsigned step = plane ? p->chroma_step : 1;
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
            hash = (hash ^ p->planes[plane][(size_t)y * p->strides[plane] + x * step]) * 16777619u;
   }
   return hash;
}

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

struct retained_picture
{
   struct retained_picture *next;
   struct texture_planar view;
   void *owner;
   void (*release)(void*);
   uint32_t hash;
};

static void release_pictures(void *userdata)
{
   struct retained_picture *held = (struct retained_picture*)userdata;
   while (held)
   {
      struct retained_picture *next = held->next;
      held->release(held->owner);
      free(held);
      held = next;
   }
}

int main(int argc, char **argv)
{
   enum image_type_enum type;
   size_t len = 0, planar_size;
   uint8_t *data, *copy = NULL, *planes = NULL;
   uint32_t *rgb = NULL, *converted = NULL;
   void *planar_stream = NULL, *rgb_stream = NULL;
   unsigned w = 0, h = 0, cw, ch, expect, frames = 0, want_flags = 0;
   int num_frames = 0, loops = 0, worst = 0, have_flags, status = 1;
   bool decoder_tight = false;
   bool require_retained = getenv("RETAIN_PLANAR") != NULL;
   struct retained_picture *held = NULL;

   if (argc < 4 || argc > 5)
   {
      fprintf(stderr, "usage: %s FILE mp4|webm EXPECT [FLAGS]\n", argv[0]);
      return 2;
   }
   type       = !strcmp(argv[2], "webm") ? IMAGE_TYPE_WEBM : IMAGE_TYPE_MP4;
   expect     = (unsigned)atoi(argv[3]);
   have_flags = argc == 5;
   if (have_flags)
      want_flags = (unsigned)strtoul(argv[4], NULL, 0);

   if (!(data = read_file(argv[1], &len)) || !(copy = (uint8_t*)malloc(len)))
   {
      fprintf(stderr, "%s: cannot read\n", argv[1]);
      goto done;
   }
   memcpy(copy, data, len);
   if (     !(planar_stream = image_transfer_anim_stream_new(data, len, type))
         || !(rgb_stream    = image_transfer_anim_stream_new(copy, len, type)))
   {
      fprintf(stderr, "%s: stream does not open\n", argv[1]);
      goto done;
   }
   image_transfer_anim_stream_get_info(planar_stream, type, &w, &h,
         &num_frames, &loops);
   if (!w || !h)
   {
      fprintf(stderr, "%s: no frame size\n", argv[1]);
      goto done;
   }

   if (     (image_transfer_anim_stream_can_planar(planar_stream, type)
               ? 1u : 0u) != expect
         || (image_transfer_anim_stream_set_planar(planar_stream, type, true)
               ? 1u : 0u) != expect)
   {
      fprintf(stderr, "%s: planar output %s, expected %s\n", argv[1],
            expect ? "refused" : "taken", expect ? "taken" : "refused");
      goto done;
   }
   if (!image_transfer_anim_stream_set_planar(planar_stream, type, expect != 0))
   {
      fprintf(stderr, "%s: planar output cannot be set back\n", argv[1]);
      goto done;
   }
   if (!expect)
   {
      printf("%s: %ux%u refuses planar output\n", argv[1], w, h);
      status = 0;
      goto done;
   }

   cw          = (w + 1) / 2;
   ch          = (h + 1) / 2;
   planar_size = IMAGE_PIXFMT_FRAME_SIZE(IMAGE_PIXFMT_I420, w, h);
   planes      = (uint8_t*)malloc(planar_size);
   rgb         = (uint32_t*)malloc((size_t)w * h * sizeof(*rgb));
   converted   = (uint32_t*)malloc((size_t)w * h * sizeof(*converted));
   if (!planes || !rgb || !converted)
      goto done;
   /* the planar frame is exactly the I420 layout: a write past it lands
    * in what ASan guards */
   if (     !image_transfer_anim_stream_set_output(planar_stream, type,
               (uint32_t*)planes)
         || !image_transfer_anim_stream_set_output(rgb_stream, type, rgb))
   {
      fprintf(stderr, "%s: output refused\n", argv[1]);
      goto done;
   }
   image_transfer_anim_stream_set_argb(rgb_stream, type, 1);

   for (;;)
   {
      unsigned flags, i;
      int duration = 0;
      const uint32_t *p, *r;
      bool borrow = (frames & 1) == 0;
      bool strided = (frames & 3) == 0;
      const struct texture_planar *view;
      struct texture_planar packed;
      if (borrow)
      {
         if (!image_transfer_anim_stream_borrow_planar(planar_stream, type, strided))
            goto done;
         memset(planes, 0xa5, planar_size);
      }
      else if (!image_transfer_anim_stream_set_planar(planar_stream, type, true))
         goto done;
      p = image_transfer_anim_stream_next(planar_stream, type, &duration);
      r = image_transfer_anim_stream_next(rgb_stream, type, &duration);
      if (!p || !r)
      {
         if (p || r)
         {
            fprintf(stderr, "%s: frame %u: planar %s, rgb %s\n", argv[1],
                  frames, p ? "decoded" : "ended", r ? "decoded" : "ended");
            goto done;
         }
         break;
      }
      if (p != (const uint32_t*)planes || r != rgb)
      {
         fprintf(stderr, "%s: frame %u not decoded into the caller's frame\n",
               argv[1], frames);
         goto done;
      }
      flags = image_transfer_anim_stream_planar_yuv(planar_stream, type);
      if (have_flags && flags != want_flags)
      {
         fprintf(stderr, "%s: frame %u reports flags %u, expected %u\n",
               argv[1], frames, flags, want_flags);
         goto done;
      }
      view = image_transfer_anim_stream_borrowed_planar(planar_stream, type);
      if (borrow && (strided || decoder_tight) && !view)
      {
         fprintf(stderr, "%s: frame %u not borrowed from decoder\n", argv[1], frames);
         goto done;
      }
      if (view)
      {
         bool tight = view->strides[0] == w && view->strides[1] == cw
            && view->strides[2] == cw;
         if (require_retained)
         {
            struct retained_picture *lease =
                  (struct retained_picture*)malloc(sizeof(*lease));
            if (!lease)
               goto done;
            if (!image_transfer_anim_stream_retain_planar(planar_stream, type,
                     &lease->owner, &lease->release))
            {
               free(lease);
               fprintf(stderr, "%s: decoder planes could not be retained\n", argv[1]);
               goto done;
            }
            lease->view = *view;
            lease->hash = planes_hash(view, w, h);
            lease->next = held;
            held = lease;
         }
         if (!frames) decoder_tight = tight;
         if (!borrow || (!strided && !tight)
               || view->planes[0] == planes || view->yuv != flags)
         {
            fprintf(stderr, "%s: frame %u not borrowed from decoder\n", argv[1], frames);
            goto done;
         }
         for (i = 0; i < planar_size; i++)
            if (planes[i] != 0xa5)
            {
               fprintf(stderr, "%s: borrowed frame copied into slot\n", argv[1]);
               goto done;
            }
      }
      else
      {
         if (view)
         {
            fprintf(stderr, "%s: borrow survived packed mode\n", argv[1]);
            goto done;
         }
         packed.planes[0] = planes;
         packed.planes[1] = planes + (size_t)w * h;
         packed.planes[2] = planes + (size_t)w * h + (size_t)cw * ch;
         packed.strides[0] = w;
         packed.strides[1] = packed.strides[2] = cw;
         packed.chroma_step = 1;
         view = &packed;
      }
      image_yuv_420_to_rgb32(converted, w,
            view->planes[0], view->strides[0],
            view->planes[1], view->strides[1],
            view->planes[2], view->strides[2],
            view->chroma_step, w, h, flags);
      for (i = 0; i < w * h; i++)
      {
         int d = channel_diff(converted[i], rgb[i], 16);
         if (channel_diff(converted[i], rgb[i], 8) > d)
            d = channel_diff(converted[i], rgb[i], 8);
         if (channel_diff(converted[i], rgb[i], 0) > d)
            d = channel_diff(converted[i], rgb[i], 0);
         if (d > worst)
            worst = d;
      }
      if (worst > TOLERANCE)
      {
         fprintf(stderr, "%s: frame %u differs from the RGB blit by %d\n",
               argv[1], frames, worst);
         goto done;
      }
      frames++;
   }
   if (!frames)
   {
      fprintf(stderr, "%s: no frames\n", argv[1]);
      goto done;
   }
   printf("%s: %ux%u, %u planar frames, flags %u, worst difference %d\n",
         argv[1], w, h, frames,
         image_transfer_anim_stream_planar_yuv(planar_stream, type), worst);
   status = 0;

done:
   if (planar_stream)
      image_transfer_anim_stream_free(planar_stream, type);
   if (rgb_stream)
      image_transfer_anim_stream_free(rgb_stream, type);
   if (!held && !status && require_retained)
      status = 1;
   {
      struct retained_picture *lease;
      for (lease = held; lease; lease = lease->next)
         if (planes_hash(&lease->view, w, h) != lease->hash)
         {
            fprintf(stderr, "%s: retained picture changed after decode and stream free\n", argv[1]);
            status = 1;
         }
   }
#ifdef HAVE_THREADS
   if (held)
   {
      sthread_t *thread = sthread_create(release_pictures, held);
      if (thread)
         sthread_join(thread);
      else
      {
         release_pictures(held);
         status = 1;
      }
   }
#else
   release_pictures(held);
#endif
   free(planes);
   free(rgb);
   free(converted);
   free(copy);
   free(data);
   return status;
}

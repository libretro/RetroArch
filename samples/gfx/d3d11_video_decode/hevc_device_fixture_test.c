/* FFmpeg reference hashes for the native HEVC device fixtures. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <formats/rmp4.h>
#include <formats/rh265.h>
#include "hevc_device_data.h"

typedef struct
{
   const char *name;
   const uint8_t *data;
   size_t size;
   const uint32_t *hash;
   unsigned width, height, depth;
} fixture_t;

#define FIXTURE(kind, depth, w, h) \
   { #kind #depth, hevc_##kind##depth, sizeof(hevc_##kind##depth), \
     hevc_##kind##depth##_hash, w, h, depth }
static const fixture_t fixtures[] = {
   FIXTURE(ip, 8, 256, 256), FIXTURE(bpyr, 8, 256, 256),
   FIXTURE(wpp, 8, 256, 256), FIXTURE(crop, 8, 250, 242),
   FIXTURE(ip, 10, 256, 256), FIXTURE(bpyr, 10, 256, 256),
   FIXTURE(wpp, 10, 256, 256), FIXTURE(crop, 10, 250, 242),
   FIXTURE(repeat, 8, 256, 256), FIXTURE(repeat, 10, 256, 256)
};

static int check_frame(rh265_video *v, const fixture_t *f, unsigned index)
{
   uint32_t hash = UINT32_C(2166136261);
   const uint8_t *data;
   int stride, width, height;
   unsigned plane, x, y, shift;
   if (index >= 12 || rh265_video_bit_depth(v) != (int)f->depth) return 0;
   for (plane = 0; plane < 3; plane++)
   {
      data = rh265_video_plane(v, (int)plane, &stride, &width, &height);
      if (!data || width != (int)(f->width >> (plane ? 1 : 0))
            || height != (int)(f->height >> (plane ? 1 : 0))) return 0;
      for (y = 0; y < (unsigned)height; y++)
         for (x = 0; x < (unsigned)width; x++)
         {
            unsigned value = f->depth == 10
               ? ((const uint16_t*)data)[y * stride + x] : data[y * stride + x];
            for (shift = 0; shift < (f->depth == 10 ? 16u : 8u); shift += 8)
               hash = (hash ^ ((value >> shift) & 255)) * UINT32_C(16777619);
         }
   }
   return hash == f->hash[index];
}

static int test_fixture(const fixture_t *f)
{
   rmp4_t *mp4 = rmp4_open_memory(f->data, f->size);
   rh265_video *v = rh265_video_open();
   rmp4_packet packet;
   static rh265_video_params_t params;
   const rmp4_track *track;
   unsigned count = 0;
   int i, selected = -1, ret, ok = 0;
   if (!mp4 || !v) goto done;
   for (i = 0; i < rmp4_num_tracks(mp4); i++)
      if (rmp4_get_track(mp4, i)->type == RMP4_TRACK_VIDEO)
      { selected = i; break; }
   if (selected < 0) goto done;
   track = rmp4_get_track(mp4, selected);
   if (rh265_video_set_extradata(v, track->codec_private, track->codec_private_size))
      goto done;
   if (rh265_video_get_params(v, 0, &params)
         || (!strncmp(f->name, "wpp", 3) && !params.entropy_coding_sync_enabled)
         || (!strncmp(f->name, "bpyr", 4) && !params.max_num_reorder_pics)) goto done;
   while ((ret = rmp4_read_packet(mp4, &packet)) == 1)
   {
      if (packet.track != selected) continue;
      ret = rh265_video_decode(v, packet.data, packet.size);
      if (ret < 0 || ret > 1) goto done;
      if (ret == 1 && !check_frame(v, f, count++)) goto done;
   }
   if (ret != 0) goto done;
   while (rh265_video_drain(v) == 0)
      if (!check_frame(v, f, count++)) goto done;
   ok = count == 12;
done:
   rh265_video_close(v);
   rmp4_close(mp4);
   printf("HEVC %s: %s (%u frames)\n", f->name, ok ? "PASS" : "FAIL", count);
   return ok;
}

int main(void)
{
   unsigned i, failures = 0;
   for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++)
      if (!test_fixture(&fixtures[i])) failures++;
   return failures ? 1 : 0;
}

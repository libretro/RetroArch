/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rvp9_hw.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static uint8_t packet[128], other[128];
static unsigned pos;
static void bits(unsigned v, unsigned n)
{
   while (n--)
   {
      packet[pos >> 3] |= ((v >> n) & 1) << (7 - (pos & 7));
      pos++;
   }
}

/* Header-complete packets; the mock accepts opaque entropy/tile bytes. */
static size_t coded(int key, int show, unsigned refresh, unsigned ref,
      int depth, int resilient, int updates)
{
   unsigned i;
   memset(packet, 0, sizeof(packet)); pos = 0;
   bits(2, 2); bits(depth == 8 ? 0 : 1, 2); bits(0, 1);
   bits(!key, 1); bits(show, 1); bits(resilient, 1);
   if (key)
   {
      bits(0x49, 8); bits(0x83, 8); bits(0x42, 8);
      if (depth != 8) bits(depth == 12, 1);
      bits(2, 3); bits(1, 1); /* BT709 full range */
      bits(63, 16); bits(47, 16);
   }
   else
   {
      if (!show) bits(0, 1); /* not intra only */
      if (!resilient) bits(0, 2);
      bits(refresh, 8);
      for (i = 0; i < 3; i++) { bits(ref, 3); bits(0, 1); }
      bits(1, 1); /* size from LAST */
   }
   bits(0, 1); /* render dimensions */
   if (!key) { bits(0, 1); bits(1, 1); }
   if (!resilient) { bits(1, 1); bits(1, 1); }
   bits(0, 2);
   bits(0, 6); bits(0, 3); bits(1, 1); bits(updates, 1);
   if (updates)
   {
      bits(1, 1); bits(3, 6); bits(1, 1); /* ref0 = -3 */
      bits(0, 3); bits(0, 2);
   }
   bits(0, 8); bits(0, 3);
   bits(updates, 1);
   if (updates)
   {
      bits(0, 1); bits(1, 1); bits(1, 1); /* map, data, abs */
      for (i = 0; i < 32; i++)
      {
         bits(i == 0, 1);
         if (!i) { bits(7, 8); bits(0, 1); }
      }
   }
   bits(0, 1); bits(1, 16);
   return (pos + 7) / 8 + 2;
}

static size_t existing(unsigned ref, int depth)
{
   memset(packet, 0, sizeof(packet)); pos = 0;
   bits(2, 2); bits(depth == 8 ? 0 : 1, 2); bits(1, 1); bits(ref, 3);
   return (pos + 7) / 8;
}

typedef struct mock
{
   rvp9_hw_picture_t picture;
   unsigned decodes, outputs, abandoned;
   int busy, output_busy, fail, invalid;
   rvp9_dec *software;
   int software_slots[32];
} mock_t;

static int decode(void *opaque, const uint8_t *data, size_t len,
      const rvp9_hw_picture_t *picture)
{
   mock_t *m = (mock_t*)opaque;
   (void)data; (void)len;
   m->decodes++;
   m->picture = *picture;
   if (m->busy) { m->busy--; return 1; }
   if (m->software)
   {
      rvp9_dec *d = m->software;
      const rvp9_hdr *h = &picture->header;
      unsigned i;
      int shown, ret;
      for (i = 0; i < RVP9_REF_FRAMES; i++)
      {
         int fb = d->ref_frame_map[i], slot = picture->reference_slots[i];
         if (slot < 0 ? (fb >= 0 && fb < RVP9_FRAME_BUFS && d->fbs[fb].valid)
               : m->software_slots[slot] != fb)
            return -1;
      }
      ret = rvp9_decode_frame(d, data, len, &shown);
      if (ret || d->hd.w != h->w || d->hd.h != h->h
            || d->hd.bit_depth != h->bit_depth || d->hd.show_frame != h->show_frame
            || d->hd.refresh_frame_flags != h->refresh_frame_flags
            || memcmp(d->lf_ref_deltas, h->lf_ref_deltas, sizeof(h->lf_ref_deltas))
            || memcmp(d->lf_mode_deltas, h->lf_mode_deltas, sizeof(h->lf_mode_deltas))
            || memcmp(d->seg_feature_enabled, h->seg_feature_enabled, sizeof(h->seg_feature_enabled))
            || memcmp(d->seg_feature_data, h->seg_feature_data, sizeof(h->seg_feature_data)))
         return -1;
      m->software_slots[picture->slot] = d->new_fb;
   }
   return m->fail ? -1 : 0;
}

static void abandon(void *opaque, uint64_t done, uint64_t value)
{
   mock_t *m = (mock_t*)opaque;
   (void)done; (void)value;
   m->abandoned++;
}

static int output(void *opaque, unsigned slot, video_hw_frame_t *frame)
{
   mock_t *m = (mock_t*)opaque;
   m->outputs++;
   if (m->output_busy) { m->output_busy--; return 1; }
   frame->image = slot + 1;
   frame->width = m->invalid == 1 ? 0 : m->picture.header.w;
   frame->height = m->picture.header.h;
   frame->pixfmt = m->picture.header.bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   if (m->invalid == 2)
      frame->pixfmt = frame->pixfmt == IMAGE_PIXFMT_P010 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
   frame->owner = m; frame->abandon = abandon;
   return 0;
}

static int run(int depth)
{
   rvp9_hw_session_t *s = (rvp9_hw_session_t*)calloc(1, sizeof(*s));
   mock_t *m = (mock_t*)calloc(1, sizeof(*m));
   rvp9_hw_ops_t ops;
   video_hw_frame_t frame, held;
   rvp9_hdr *saved, *parsed;
   size_t len, i;
   unsigned calls;
   int first, last, map[8];
   CHECK(s && m);
   saved = (rvp9_hdr*)calloc(1, sizeof(*saved));
   parsed = (rvp9_hdr*)calloc(1, sizeof(*parsed));
   CHECK(saved && parsed);
   ops.decoder = m; ops.decode = decode; ops.output = output;
   CHECK(!rvp9_hw_session_init(s, &ops, 9));
   CHECK(!rvp9_hw_session_init(s, &ops, 33));
   CHECK(rvp9_hw_session_init(s, &ops, 10));
   len = existing(0, depth);
   CHECK(rvp9_hw_session_submit(s, packet, len) < 0 && !m->decodes);
   len = coded(1, 1, 255, 0, depth, 0, 1);
   for (i = 0; i < len - 1; i++)
      CHECK(rvp9_parse_frame_header(packet, i, NULL, NULL, parsed) < 0);
   CHECK(rvp9_parse_frame_header(packet, len, NULL, NULL, parsed) == 0);
   CHECK(parsed->bit_depth == depth && parsed->color_space == 2 && parsed->color_range == 1);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
   first = s->previous_slot;
   for (i = 0; i < 8; i++) CHECK(s->reference_slots[i] == first);
   CHECK(s->header.lf_ref_deltas[0] == -3 && s->header.lf_ref_deltas[2] == -1);
   CHECK(s->header.seg_feature_enabled[0][0] && s->header.seg_feature_data[0][0] == 7);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 1 && m->decodes == 1);
   m->output_busy = 1;
   CHECK(rvp9_hw_session_output(s, &frame) == 1 && !frame.image);
   CHECK(rvp9_hw_session_output(s, &held) == 0 && held.image == (uint64_t)first + 1);
   CHECK(rvp9_hw_session_output(s, &frame) == 1);
   *saved = s->header; memcpy(map, s->reference_slots, sizeof(map));
   len = coded(0, 0, 128, 0, depth, 0, 0);
   memcpy(other, packet, len); m->busy = 2;
   CHECK(rvp9_hw_session_submit(s, packet, len) == 1);
   CHECK(!memcmp(saved, &s->header, sizeof(*saved)) && !memcmp(map, s->reference_slots, sizeof(map)));
   calls = m->decodes;
   CHECK(rvp9_hw_session_submit(s, other, len) < 0 && m->decodes == calls);
   CHECK(rvp9_hw_session_submit(s, packet, len - 1) < 0 && m->decodes == calls);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 1);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
   last = s->previous_slot;
   CHECK(last != first && s->reference_slots[7] == last && s->reference_slots[0] == first);
   CHECK(s->header.color_space == 2 && s->header.color_range == 1 && s->header.subsampling_x == 1);
   CHECK(s->header.lf_ref_deltas[0] == -3 && s->header.seg_feature_data[0][0] == 7);
   CHECK(rvp9_hw_session_output(s, &frame) == 1);
   calls = m->decodes;
   len = existing(7, depth);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0 && m->decodes == calls);
   CHECK(rvp9_hw_session_output(s, &frame) == 0 && frame.image == (uint64_t)last + 1);
   video_hw_frame_abandon(&frame);
   CHECK(held.image == (uint64_t)first + 1); video_hw_frame_abandon(&held);
   CHECK(m->abandoned == 2);
   len = coded(0, 1, 1, 7, depth, 0, 0);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
   CHECK(!m->picture.use_prev_frame_mvs && m->picture.previous_slot == last);
   CHECK(m->picture.slot != last && m->picture.slot != first);
   for (i = 0; i < 3; i++) CHECK(m->picture.named_slots[i] == last);
   CHECK(rvp9_hw_session_output(s, &frame) == 0); video_hw_frame_abandon(&frame);
   len = coded(0, 1, 1, 7, depth, 0, 0);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0 && m->picture.use_prev_frame_mvs);
   CHECK(rvp9_hw_session_output(s, &frame) == 0); video_hw_frame_abandon(&frame);
   *saved = s->header;
   len = coded(0, 1, 1, 7, depth, 1, 0); m->busy = 1;
   CHECK(rvp9_hw_session_submit(s, packet, len) == 1);
   CHECK(rvp9_hw_session_cancel(s) && !rvp9_hw_session_cancel(s));
   CHECK(!memcmp(saved, &s->header, sizeof(*saved)));
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
   CHECK(s->header.lf_ref_deltas[0] == 1 && !s->header.seg_feature_enabled[0][0]);
   CHECK(rvp9_hw_session_output(s, &frame) == 0); video_hw_frame_abandon(&frame);
   calls = m->outputs;
   len = existing(7, depth);
   CHECK(!rvp9_hw_session_submit(s, packet, len));
   CHECK(rvp9_hw_session_discard_output(s) && !rvp9_hw_session_discard_output(s));
   CHECK(m->outputs == calls && rvp9_hw_session_output(s, &frame) == 1);
   for (i = 0; i < 32; i++)
   {
      unsigned j;
      memcpy(map, s->reference_slots, sizeof(map)); last = s->previous_slot;
      len = coded(0, 1, i < 8 ? 1u << i : 0, (unsigned)(i + 3) % 8, depth, 0, 0);
      CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
      for (j = 0; j < 8; j++) CHECK(s->previous_slot != map[j]);
      CHECK(s->previous_slot != last);
      CHECK(rvp9_hw_session_output(s, &frame) == 0); video_hw_frame_abandon(&frame);
   }
   *saved = s->header; calls = m->decodes;
   len = coded(1, 1, 255, 0, depth == 8 ? 10 : 8, 0, 0);
   CHECK(rvp9_hw_session_submit(s, packet, len) == -2 && calls == m->decodes);
   CHECK(!memcmp(saved, &s->header, sizeof(*saved)));
   len = coded(1, 1, 255, 0, 12, 0, 0);
   CHECK(rvp9_hw_session_submit(s, packet, len) == -2 && calls == m->decodes);
   len = coded(1, 1, 255, 0, 8, 0, 0);
   CHECK(!rvp9_parse_frame_header(packet, len, saved, NULL, parsed) && parsed->bit_depth == 8);
   len = coded(0, 1, 1, 0, depth, 0, 0);
   CHECK(rvp9_hw_session_submit(s, packet, len) == 0);
   calls = m->abandoned;
   m->invalid = 1;
   CHECK(rvp9_hw_session_output(s, &frame) < 0 && !frame.image && m->abandoned == calls + 1);
   CHECK(s->broken && rvp9_hw_session_submit(s, packet, len) < 0);
   CHECK(rvp9_hw_session_init(s, &ops, 10)); m->invalid = 0;
   len = coded(1, 0, 255, 0, depth, 0, 0); m->fail = 1;
   CHECK(rvp9_hw_session_submit(s, packet, len) < 0 && s->broken && !s->started);
   CHECK(rvp9_hw_session_init(s, &ops, 10)); m->fail = 0; m->invalid = 2;
   len = coded(1, 1, 255, 0, depth, 0, 0);
   CHECK(!rvp9_hw_session_submit(s, packet, len));
   CHECK(rvp9_hw_session_output(s, &frame) < 0 && s->broken && !frame.image);
   free(saved); free(parsed); free(m); free(s);
   return 0;
}

static unsigned read32(const uint8_t *p)
{
   return (unsigned)p[0] | ((unsigned)p[1] << 8)
         | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int stream(const char *path)
{
   FILE *f = fopen(path, "rb");
   rvp9_hw_session_t *s = (rvp9_hw_session_t*)calloc(1, sizeof(*s));
   mock_t *m = (mock_t*)calloc(1, sizeof(*m));
   rvp9_hw_ops_t ops;
   video_hw_frame_t frame;
   uint8_t header[32], *data;
   unsigned count = 0, size;
   size_t got;
   CHECK(f && s && m);
   CHECK(fread(header, 1, 32, f) == 32 && !memcmp(header, "DKIF", 4)
         && !memcmp(header + 8, "VP90", 4));
   m->software = (rvp9_dec*)calloc(1, sizeof(*m->software));
   CHECK(m->software);
   ops.decoder = m; ops.decode = decode; ops.output = output;
   CHECK(rvp9_hw_session_init(s, &ops, 10));
   while ((got = fread(header, 1, 12, f)) != 0)
   {
      CHECK(got == 12);
      size = read32(header);
      CHECK(size && size <= 16 * 1024 * 1024);
      data = (uint8_t*)malloc(size);
      CHECK(data && fread(data, 1, size, f) == size);
      m->busy = 1;
      CHECK(rvp9_hw_session_submit(s, data, size) == 1);
      CHECK(rvp9_hw_session_submit(s, data, size) == 0);
      CHECK(rvp9_hw_session_output(s, &frame) == 0);
      video_hw_frame_abandon(&frame);
      free(data); count++;
   }
   CHECK(feof(f) && count && m->abandoned == count);
   printf("VP9 hardware metadata matches software on %u coded frames: %s\n", count, path);
   fclose(f); rvp9_free(m->software); free(m->software); free(m); free(s);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc == 2) return stream(argv[1]);
   if (run(8) || run(10)) return 1;
   puts("VP9 hardware-session metadata, retries and lease tests passed (8/10-bit)");
   return 0;
}

/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/rvp9_hw.h>
#include <formats/video_player.h>
#include "../../../formats/webm/rwebm_video.c"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static uint8_t file[4096], packet[64], key[64], hidden[64], shown[64];
static size_t used, prefix;
static unsigned bitpos, tail_hidden;
static unsigned lives, opens, closes, calls, decodes, exports, abandoned;

static void bits(unsigned v, unsigned n)
{
   while (n--) { packet[bitpos >> 3] |= ((v >> n) & 1) << (7 - (bitpos & 7)); bitpos++; }
}
static size_t coded(int intra, int show)
{
   unsigned i;
   memset(packet, 0, sizeof(packet)); bitpos = 0;
   bits(2, 2); bits(0, 2); bits(0, 1); bits(!intra, 1); bits(show, 1); bits(1, 1);
   if (intra)
   {
      bits(0x49, 8); bits(0x83, 8); bits(0x42, 8); bits(2, 3); bits(0, 1);
      bits(63, 16); bits(47, 16);
   }
   else
   {
      if (!show) bits(0, 1);
      bits(1, 8);
      for (i = 0; i < 3; i++) { bits(0, 3); bits(0, 1); }
      bits(1, 1);
   }
   bits(0, 1);
   if (!intra) { bits(0, 1); bits(1, 1); }
   bits(0, 2); bits(0, 6); bits(0, 3); bits(0, 1);
   bits(0, 8); bits(0, 3); bits(0, 1); bits(0, 1); bits(1, 16);
   return (bitpos + 7) / 8 + 2;
}
static size_t start(unsigned id, unsigned bytes)
{
   size_t marker;
   while (bytes--) file[used++] = (uint8_t)(id >> (bytes * 8));
   marker = used; file[used++] = 0x40; file[used++] = 0;
   return marker;
}
static void finish(size_t marker)
{
   size_t n = used - marker - 2;
   file[marker] = (uint8_t)(0x40 | (n >> 8)); file[marker + 1] = (uint8_t)n;
}
static void integer(unsigned id, unsigned bytes, unsigned v)
{
   size_t marker = start(id, bytes);
   file[used++] = (uint8_t)v; finish(marker);
}
static void block(unsigned ms, unsigned flags, const uint8_t *data, size_t n)
{
   size_t marker = start(0xa3, 1);
   file[used++] = 0x81; file[used++] = (uint8_t)(ms >> 8);
   file[used++] = (uint8_t)ms; file[used++] = (uint8_t)flags;
   memcpy(file + used, data, n); used += n; finish(marker);
}
static void fixture(unsigned matrix, unsigned range, unsigned transfer)
{
   size_t m, segment, tracks, track, video, colour, cluster, nk, nh, ns, n;
   uint8_t super[192], lace[3] = {1, 0x88, 0x88};
   used = 0;
   m = start(0x1a45dfa3, 4); finish(m);
   segment = start(0x18538067, 4);
   tracks = start(0x1654ae6b, 4); track = start(0xae, 1);
   integer(0xd7, 1, 1); integer(0x83, 1, 1);
   m = start(0x86, 1); memcpy(file + used, "V_VP9", 5); used += 5; finish(m);
   video = start(0xe0, 1); integer(0xb0, 1, 64); integer(0xba, 1, 48);
   colour = start(0x55b0, 2); integer(0x55b1, 2, matrix);
   integer(0x55b9, 2, range); integer(0x55ba, 2, transfer);
   integer(0x55bb, 2, 1); integer(0x55bc, 2, 200);
   integer(0x55bd, 2, 80);
   {
      unsigned j;
      size_t master = start(0x55d0, 2);
      for (j = 0; j < 10; j++)
      {
         uint32_t value = j < 8 ? 0x3f000000u : j == 8 ? 0x44800000u : 0;
         size_t f = start(0x55d1 + j, 2);
         file[used++] = (uint8_t)(value >> 24);
         file[used++] = (uint8_t)(value >> 16);
         file[used++] = (uint8_t)(value >> 8);
         file[used++] = (uint8_t)value;
         finish(f);
      }
      finish(master);
   }
   finish(colour);
   finish(video); finish(track); finish(tracks);
   cluster = start(0x1f43b675, 4); integer(0xe7, 1, 0);
   nk = coded(1, 1); memcpy(key, packet, nk);
   nh = coded(0, 0); memcpy(hidden, packet, nh);
   ns = coded(0, 1); memcpy(shown, packet, ns);
   n = 0; memcpy(super + n, key, nk); n += nk;
   if (tail_hidden)
   {
      memcpy(super + n, shown, ns); n += ns;
      memcpy(super + n, hidden, nh); n += nh;
   }
   else
   {
      memcpy(super + n, hidden, nh); n += nh;
      memcpy(super + n, shown, ns); n += ns;
   }
   super[n++] = 0xc2; super[n++] = (uint8_t)nk;
   super[n++] = (uint8_t)(tail_hidden ? ns : nh);
   super[n++] = (uint8_t)(tail_hidden ? nh : ns); super[n++] = 0xc2;
   block(0, 0x80, super, n);
   block(40, 0, lace + 1, 1); prefix = used;
   block(80, 4, lace, sizeof(lace));
   finish(cluster); finish(segment);
}

typedef struct mock
{
   const uint8_t *retry;
   size_t size;
   unsigned refs, width, height;
   int output_busy;
} mock_t;
static void release(mock_t *m)
{
   if (!--m->refs) { lives--; free(m); }
}
static void abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)done; (void)value; abandoned++; release((mock_t*)owner);
}
static int decode(void *owner, const uint8_t *data, size_t size, const rvp9_hw_picture_t *p)
{
   mock_t *m = (mock_t*)owner;
   calls++;
   if (!m->retry) { m->retry = data; m->size = size; return 1; }
   if (data != m->retry || size != m->size || p->slot < 0) return -1;
   m->retry = NULL; decodes++; return 0;
}
static int output(void *owner, unsigned slot, video_hw_frame_t *out)
{
   mock_t *m = (mock_t*)owner;
   if (!m->output_busy) { m->output_busy = 1; return 1; }
   m->output_busy = 0; m->refs++;
   memset(out, 0, sizeof(*out)); out->image = ++exports; out->wait = slot + 1;
   out->width = m->width; out->height = m->height; out->pixfmt = IMAGE_PIXFMT_NV12;
   out->owner = m; out->abandon = abandon;
   return 0;
}
static bool open_backend(void *user, const rvp9_hdr *h, unsigned slots, rvp9_hw_ops_t *ops)
{
   mock_t *m;
   (void)user;
   if (h->bit_depth != 8 || slots != 10) return false;
   m = (mock_t*)calloc(1, sizeof(*m));
   if (!m) return false;
   m->refs = 1; m->width = h->w; m->height = h->h; lives++; opens++;
   ops->decoder = m; ops->decode = decode; ops->output = output;
   return true;
}
static void close_backend(rvp9_hw_ops_t *ops)
{
   closes++; release((mock_t*)ops->decoder); memset(ops, 0, sizeof(*ops));
}
static const rvp9_hw_backend_t backend = {NULL, open_backend, close_backend};
static unsigned gpu_starts, cpu_starts, presents;
static int start_cpu(void *user, unsigned w, unsigned h, unsigned slots)
{
   (void)user; (void)w; (void)h; (void)slots; cpu_starts++; return 0;
}
static int start_gpu(void *user, unsigned w, unsigned h, unsigned slots)
{
   (void)user;
   if (w != 64 || h != 48 || slots != 1) return -1;
   gpu_starts++; return 0;
}
static enum video_player_present present(void *user, const video_player_frame_t *f)
{
   (void)user;
   if (!f->hw || f->pixels || f->how != VIDEO_PLAYER_DIRECT)
      return VIDEO_PLAYER_PRESENT_FAILED;
   presents++; video_hw_frame_abandon(f->hw); return VIDEO_PLAYER_PRESENT_DONE;
}

int main(void)
{
   rwebm_video_stream_t *s;
   rwebm_t *demux;
   rwebm_packet pkt;
   video_hw_frame_t frame, held;
   unsigned frames = 0, tries, waits = 0;
   size_t first_floor = 0, consumed;
   int duration = -1, need_more;
   video_player_t *player;
   video_player_sink_t sink;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   fixture(1, 2, 1);
   demux = rwebm_open_memory(file, used); CHECK(demux);
   CHECK(rwebm_read_packet(demux, &pkt) == 1 && rwebm_read_packet(demux, &pkt) == 1);
   CHECK(rwebm_read_packet(demux, &pkt) == 1);
   consumed = rwebm_consumed(demux);
   CHECK(consumed == (size_t)(pkt.data - file) + pkt.size && consumed < rwebm_tell(demux));
   CHECK(rwebm_read_packet(demux, &pkt) == 1 && pkt.data == file + consumed);
   rwebm_close(demux);
   s = rwebm_video_stream_open_avail(file, used, prefix, &need_more); CHECK(s && !s->frame);
   rvp9_set_hw_backend(NULL);
   CHECK(!image_transfer_anim_stream_set_hw(s, IMAGE_TYPE_WEBM, true) && s->vp9 && !s->pkt_idx);
   rvp9_set_hw_backend(&backend);
   CHECK(image_transfer_anim_stream_set_hw(s, IMAGE_TYPE_WEBM, true) && !s->vp9 && opens == 1);
   memset(&held, 0, sizeof(held));
   for (tries = 0; tries < 100 && frames < 4; tries++)
   {
      const uint32_t *token = image_transfer_anim_stream_next(s, IMAGE_TYPE_WEBM, &duration);
      if (!token)
      {
         CHECK(image_transfer_anim_stream_waiting(s, IMAGE_TYPE_WEBM)); waits++;
         if (s->pending)
         {
            CHECK(rwebm_video_stream_consumed(s) <= (size_t)(s->pending_packet.data - file));
            if (!frames)
            {
               if (!first_floor) first_floor = rwebm_video_stream_consumed(s);
               CHECK(rwebm_video_stream_consumed(s) == first_floor);
            }
         }
         else { CHECK(frames == 2); rwebm_video_stream_set_avail(s, used); }
         continue;
      }
      CHECK(token == &s->hw_marker && !s->frame);
      CHECK(image_transfer_anim_stream_hw_frame(s, IMAGE_TYPE_WEBM, &frame));
      CHECK(frame.yuv == (IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_FULL_RANGE));
      CHECK(frame.color.primaries == 1 && frame.color.transfer == 1
            && frame.color.matrix == 1 && frame.color.range == 2
            && frame.color.max_cll == 200 && frame.color.max_fall == 80
            && frame.color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
            && frame.color.mastering.chromaticity[0] == 25000
            && frame.color.mastering.luminance_max == 10240000);
      CHECK(!image_transfer_anim_stream_hw_frame(s, IMAGE_TYPE_WEBM, &held) || !held.image);
      if (!frames) held = frame;
      else video_hw_frame_abandon(&frame);
      frames++;
      if (frames == 2) rvp9_set_hw_backend(NULL);
   }
   CHECK(frames == 4 && waits >= 7 && calls == 6 && decodes == 3 && exports == 4);
   CHECK(!image_transfer_anim_stream_next(s, IMAGE_TYPE_WEBM, NULL)
         && !image_transfer_anim_stream_waiting(s, IMAGE_TYPE_WEBM));
   rvp9_set_hw_backend(&backend);
   rwebm_video_stream_rewind(s); CHECK(opens == 2 && closes == 1);
   for (tries = 0; tries < 30; tries++)
      if (rwebm_video_stream_next(s, NULL)) break;
   CHECK(tries < 30 && s->hw_untaken && held.image);
   rwebm_video_stream_close(s);
   CHECK(closes == 2 && lives == 1);
   video_hw_frame_abandon(&held);
   CHECK(lives == 0 && abandoned == exports);
   fixture(9, 2, 16);
   s = rwebm_video_stream_open(file, used); CHECK(s);
   CHECK(!rwebm_video_stream_set_hw(s, 1) && s->vp9 && opens == 2);
   rwebm_video_stream_close(s);
   fixture(1, 2, 1);
   s = rwebm_video_stream_open(file, used); CHECK(s);
   memset(&sink, 0, sizeof(sink));
   sink.start = start_cpu; sink.start_hw = start_gpu; sink.present = present;
   player = video_player_new(s, IMAGE_TYPE_WEBM, 1, &sink, VIDEO_PLAYER_INLINE); CHECK(player);
   for (tries = 0; tries < 1000 && state == VIDEO_PLAYER_PLAYING; tries++)
      state = video_player_poll(player, (int64_t)tries * 10000);
   CHECK(state == VIDEO_PLAYER_ENDED && gpu_starts == 1 && !cpu_starts && presents == 4);
   video_player_free(player); rwebm_video_stream_close(s);
   CHECK(!lives && opens == 3 && closes == 3 && abandoned == exports);
   tail_hidden = 1;
   fixture(1, 2, 1);
   s = rwebm_video_stream_open_avail(file, used, prefix, &need_more); CHECK(s);
   CHECK(rwebm_video_stream_set_hw(s, 1));
   {
      unsigned before = exports, decoded = decodes;
      int ret = 2;
      for (tries = 0; tries < 30 && ret == 2; tries++)
      {
         ret = rwebm_video_stream_skip(s, &duration);
         CHECK(exports == before);
         if (ret == 2)
         {
            CHECK(rwebm_video_stream_waiting(s) && s->pending);
            CHECK(rwebm_video_stream_consumed(s) <= (size_t)(s->pending_packet.data - file));
         }
      }
      CHECK(ret == 1 && decodes == decoded + 3 && duration == 40);
      CHECK(!rwebm_video_stream_hw_frame(s, &frame));
      for (tries = 0; tries < 30; tries++)
         if (rwebm_video_stream_next(s, NULL)) break;
      CHECK(tries < 30 && rwebm_video_stream_hw_frame(s, &frame));
      CHECK(exports == before + 1 && decodes == decoded + 3);
      held = frame;
      CHECK(rwebm_video_stream_skip(s, NULL) == 2 && !s->pending);
      rwebm_video_stream_set_avail(s, used);
      CHECK(rwebm_video_stream_skip(s, &duration) == 1 && exports == before + 1);
      CHECK(!rwebm_video_stream_hw_frame(s, &frame));
      for (tries = 0; tries < 30; tries++)
         if (rwebm_video_stream_next(s, NULL)) break;
      CHECK(tries < 30 && rwebm_video_stream_hw_frame(s, &frame));
      video_hw_frame_abandon(&frame);
      CHECK(exports == before + 2 && rwebm_video_stream_skip(s, NULL) == 0);
      rwebm_video_stream_rewind(s);
      CHECK(rwebm_video_stream_skip(s, NULL) == 2 && s->pending);
      rwebm_video_stream_rewind(s);
      for (tries = 0; tries < 30; tries++)
         if (rwebm_video_stream_next(s, NULL)) break;
      CHECK(tries < 30 && rwebm_video_stream_hw_frame(s, &frame));
      video_hw_frame_abandon(&frame);
   }
   rwebm_video_stream_close(s);
   CHECK(lives == 1 && held.image);
   video_hw_frame_abandon(&held);
   CHECK(!lives && opens == closes && abandoned == exports);
   rvp9_set_hw_backend(NULL);
   puts("WebM hardware dispatch, superframe retries, lacing, partial input and leases passed");
   return 0;
}

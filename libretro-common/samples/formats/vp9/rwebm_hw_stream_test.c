/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/rvp9_hw.h>
#include <formats/video_player.h>
#include "../../../formats/webm/rwebm_video.c"

/* The extension must preserve the backend prefix already published. */
typedef struct rvp9_backend_v1
{
   void *user;
   bool (*open)(void *, const rvp9_hdr *, unsigned, rvp9_hw_ops_t *);
   void (*close)(rvp9_hw_ops_t *);
   bool packet_decoder;
} rvp9_backend_v1_t;
typedef char rvp9_backend_mode_offset_preserved[
   offsetof(rvp9_hw_backend_t, packet_decoder)
      == offsetof(rvp9_backend_v1_t, packet_decoder) ? 1 : -1];
typedef char rvp9_backend_callback_appended[
   offsetof(rvp9_hw_backend_t, open_color) >= sizeof(rvp9_backend_v1_t) ? 1 : -1];
typedef char rvp9_backend_restart_appended[
   offsetof(rvp9_hw_backend_t, restart) >= offsetof(rvp9_hw_backend_t, open_config)
      + sizeof(((rvp9_hw_backend_t *)0)->open_config) ? 1 : -1];

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static uint8_t file[4096], packet[64], key[64], hidden[64], shown[64];
static size_t used, prefix;
static unsigned bitpos, tail_hidden, terminal_mode;
static unsigned fixture_chroma_horz, fixture_chroma_vert;
static unsigned fixture_full_range, fixture_color_space;
static unsigned terminal_packets = 7, terminal_shown = 3;
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
      bits(0x49, 8); bits(0x83, 8); bits(0x42, 8); bits(fixture_color_space, 3); bits(fixture_full_range, 1);
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
   fixture_full_range = range == 2;
   fixture_color_space = matrix == 9 ? 5 : 2;
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
   if (fixture_chroma_horz == 257)
   {
      static const uint8_t overflow[9] = {1, 0, 0, 0, 0, 0, 0, 0, 1};
      m = start(0x55b7, 2);
      memcpy(file + used, overflow, sizeof(overflow));
      used += sizeof(overflow);
      finish(m);
   }
   else if (fixture_chroma_horz == 256)
   {
      static const uint8_t overflow[8] = {0, 0, 0, 1, 0, 0, 0, 1};
      m = start(0x55b7, 2);
      memcpy(file + used, overflow, sizeof(overflow));
      used += sizeof(overflow);
      finish(m);
   }
   else if (fixture_chroma_horz)
      integer(0x55b7, 2, fixture_chroma_horz);
   if (fixture_chroma_vert)
      integer(0x55b8, 2, fixture_chroma_vert);
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
   if (terminal_mode) block(80, 0x80, super, n);
   else block(80, 4, lace, sizeof(lace));
   finish(cluster); finish(segment);
}

typedef struct mock
{
   const uint8_t *retry;
   size_t size;
   unsigned refs, width, height;
   int output_busy;
   unsigned native_count, eos_attempts, eos_accepted, shown, restarts;
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
   m->retry = NULL; decodes++; m->native_count++; return 0;
}
static int output(void *owner, unsigned slot, video_hw_frame_t *out)
{
   mock_t *m = (mock_t*)owner;
   if (terminal_mode && m->native_count >= terminal_packets - 1 && !m->eos_accepted) return 1;
   if (!m->output_busy) { m->output_busy = 1; return 1; }
   m->output_busy = 0; m->refs++; m->shown++;
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
static int restart_backend(void *owner)
{
   mock_t *m = (mock_t *)owner;
   if (++m->restarts & 1) return 1;
   m->retry = NULL;
   m->size = 0;
   m->output_busy = 0;
   m->native_count = m->eos_attempts = m->eos_accepted = m->shown = 0;
   return 0;
}
static const rvp9_hw_backend_t backend = {
   NULL, open_backend, close_backend, false, NULL, NULL, NULL, NULL
};
static video_color_metadata_t admitted_color;
static unsigned color_opens, color_refuse, color_partial;

static bool open_color_backend(void *user, const rvp9_hdr *h,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops)
{
   color_opens++;
   if (color)
      admitted_color = *color;
   else
      memset(&admitted_color, 0, sizeof(admitted_color));
   if (color_refuse && !color_partial)
      return false;
   if (!open_backend(user, h, slots, ops))
      return false;
   if (color_partial)
      ops->output = NULL;
   return !color_refuse;
}

static int color_admission(void)
{
   static const rvp9_hw_backend_t colors = {
      NULL, open_backend, close_backend, true, open_color_backend, NULL, NULL, NULL
   };
   rwebm_video_stream_t *s;
   rvp9_hw_ops_t ops;
   rvp9_hw_backend_t snapshot;
   rvp9_hdr header;
   rvp9_hw_backend_t color_only = colors;
   rvp9_hw_backend_t replacement = backend;
   unsigned before, closed;
   memset(&header, 0, sizeof(header));
   header.bit_depth = 8;
   header.w = 64;
   header.h = 48;
   rvp9_set_hw_backend(&backend);
   CHECK(!rvp9_unset_hw_backend(NULL) && !rvp9_unset_hw_backend(&colors));
   CHECK(rvp9_hw_backend_open(&header, 10, &ops, &snapshot));
   snapshot.close(&ops);
   CHECK(rvp9_unset_hw_backend(&backend) && !rvp9_unset_hw_backend(&backend));
   CHECK(!rvp9_hw_backend_open(&header, 10, &ops, &snapshot));
   replacement.user = &header;
   rvp9_set_hw_backend(&replacement);
   CHECK(!rvp9_unset_hw_backend(&backend));
   CHECK(rvp9_hw_backend_open(&header, 10, &ops, &snapshot));
   CHECK(snapshot.user == &header);
   snapshot.close(&ops);
   CHECK(rvp9_unset_hw_backend(&replacement));
   color_only.open = NULL;
   rvp9_set_hw_backend(&color_only);
   CHECK(rvp9_hw_backend_open(&header, 10, &ops, &snapshot));
   CHECK(!admitted_color.transfer && snapshot.open_color == open_color_backend);
   snapshot.close(&ops);
   tail_hidden = 0;
   fixture(1, 2, 1);
   s = rwebm_video_stream_open(file, used);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   CHECK(admitted_color.primaries == 1 && admitted_color.matrix == 1
         && admitted_color.transfer == 1 && admitted_color.range == 2
         && admitted_color.max_cll == 200 && admitted_color.max_fall == 80
         && admitted_color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
         && admitted_color.mastering.luminance_max == 10240000);
   CHECK(s->hw->packet_decoder && !memcmp(&s->hw_color,
         &admitted_color, sizeof(admitted_color)));
   rvp9_set_hw_backend(NULL);
   rwebm_video_stream_close(s);
   CHECK(!lives);
   rvp9_set_hw_backend(&colors);
   for (color_partial = 0; color_partial < 2; color_partial++)
   {
      color_refuse = 1;
      before = opens;
      closed = closes;
      s = rwebm_video_stream_open(file, used);
      CHECK(s && !rwebm_video_stream_set_hw(s, 1) && s->vp9 && !s->hw);
      CHECK(opens == before + color_partial && closes == closed + color_partial);
      rwebm_video_stream_close(s);
      CHECK(!lives);
   }
   color_refuse = 0;
   color_partial = 1;
   CHECK(!rvp9_hw_backend_open(&header, 10, &ops, &snapshot));
   CHECK(!ops.decoder && !snapshot.close && !lives);
   color_partial = 0;
   CHECK(color_opens == 5);
   rvp9_set_hw_backend(NULL);
   return 0;
}

static unsigned config_opens;
static rvp9_config_t admitted_config;
static bool open_config_backend(void *user, const rvp9_hdr *header,
      const video_color_metadata_t *color, const rvp9_config_t *config,
      unsigned slots, rvp9_hw_ops_t *ops)
{
   config_opens++;
   if (config)
      admitted_config = *config;
   return open_color_backend(user, header, color, slots, ops);
}

static int config_admission(void)
{
   static const uint8_t bytes[12] = {1, 0, 0, 0, 0, 0, 0x82, 1, 1, 1, 0, 0};
   rvp9_hw_backend_t offer, snapshot;
   rvp9_hw_ops_t ops;
   rvp9_hdr header;
   rvp9_config_t config;
   video_color_metadata_t color;
   rwebm_video_stream_t *stream;
   const rwebm_track *track;
   unsigned before;
   memset(&offer, 0, sizeof(offer));
   memset(&header, 0, sizeof(header));
   memset(&color, 0, sizeof(color));
   header.w = 64;
   header.h = 48;
   header.bit_depth = 8;
   header.subsampling_x = header.subsampling_y = 1;
   color.max_cll = 777;
   color.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
   color.mastering.luminance_max = 10000000;
   offer.open_color = open_color_backend;
   offer.close = close_backend;
   offer.packet_decoder = true;
   CHECK(rvp9_config_parse(bytes, sizeof(bytes), &config));
   rvp9_set_hw_backend(&offer);
   before = color_opens;
   config.chroma = 0;
   CHECK(!rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(color_opens == before && !ops.decoder && !snapshot.close);
   config.chroma = 1;
   CHECK(rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(admitted_color.transfer == 1 && admitted_color.range == 1
         && admitted_color.max_cll == 777
         && admitted_color.mastering.luminance_max == 10000000);
   snapshot.close(&ops);
   offer.open_config = open_config_backend;
   CHECK(rvp9_hw_backend_open_color(&header, &color, 10, &ops, &snapshot));
   CHECK(!config_opens);
   snapshot.close(&ops);
   offer.open_config = NULL;
   color.transfer = 16;
   before = color_opens;
   CHECK(!rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(color_opens == before);
   color.transfer = 0;
   config.profile = 2;
   CHECK(!rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   config.profile = 0;
   config.full_range = 1;
   CHECK(!rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(color_opens == before);
   config.full_range = 0;
   config.chroma = 0;
   offer.open_color = NULL;
   offer.open_config = open_config_backend;
   CHECK(rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(config_opens == 1 && !admitted_config.chroma);
   snapshot.close(&ops);
   color_partial = 1;
   CHECK(!rvp9_hw_backend_open_config(&header, &color, &config, 10, &ops, &snapshot));
   CHECK(!ops.decoder && !snapshot.close && !lives);
   color_partial = 0;
   CHECK(rvp9_hw_backend_open_color(&header, &color, 10, &ops, &snapshot));
   snapshot.close(&ops);
   fixture_chroma_horz = 1;
   fixture_chroma_vert = 2;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream);
   track = rwebm_get_track(stream->demux, stream->track);
   CHECK(track && track->chroma_siting_horz == 1 && track->chroma_siting_vert == 2);
   CHECK(rwebm_video_stream_set_hw(stream, 1) && !admitted_config.chroma);
   rwebm_video_stream_close(stream);
   fixture_chroma_vert = 1;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && rwebm_video_stream_set_hw(stream, 1) && admitted_config.chroma == 1);
   rwebm_video_stream_close(stream);
   fixture_chroma_horz = 2;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && !rwebm_video_stream_set_hw(stream, 1) && stream->vp9);
   rwebm_video_stream_close(stream);
   fixture_chroma_vert = 2;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && !rwebm_video_stream_set_hw(stream, 1) && stream->vp9);
   rwebm_video_stream_close(stream);
   fixture_chroma_vert = 1;
   fixture_chroma_horz = 255;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && !rwebm_video_stream_set_hw(stream, 1));
   rwebm_video_stream_close(stream);
   fixture_chroma_horz = 256;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && !rwebm_video_stream_set_hw(stream, 1));
   track = rwebm_get_track(stream->demux, stream->track);
   CHECK(track && track->chroma_siting_horz == 3);
   rwebm_video_stream_close(stream);
   fixture_chroma_horz = 257;
   fixture(1, 1, 1);
   stream = rwebm_video_stream_open(file, used);
   CHECK(stream && !rwebm_video_stream_set_hw(stream, 1));
   track = rwebm_get_track(stream->demux, stream->track);
   CHECK(track && track->chroma_siting_horz == 3);
   rwebm_video_stream_close(stream);
   fixture_chroma_horz = fixture_chroma_vert = 0;
   rvp9_set_hw_backend(NULL);
   CHECK(!lives);
   return 0;
}

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

static int seek_hardware(int reuse)
{
   static const int64_t targets[] = {40000000, 80000000, 0, 60000000};
   rvp9_hw_backend_t offer = backend;
   rwebm_video_stream_t *s;
   video_hw_frame_t frame, held;
   unsigned i, tries, before, decoded;
   unsigned started = opens;
   int result, need_more;
   int64_t position;
   tail_hidden = 0;
   offer.restart = reuse ? restart_backend : NULL;
   rvp9_set_hw_backend(&offer);
   fixture(1, 2, 1);
   s = rwebm_video_stream_open_avail(file, used, prefix, &need_more);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   CHECK(rwebm_video_stream_seekable(s));
   memset(&held, 0, sizeof(held));
   for (i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
   {
      before = exports;
      decoded = decodes;
      CHECK(rwebm_video_stream_seek_begin_ns(s, targets[i]));
      for (tries = 0; tries < 100; tries++)
      {
         result = rwebm_video_stream_seek_step(s, &position);
         CHECK(result >= 0 && exports == before);
         if (result == 1) break;
         if (s->seek_scan) CHECK(decodes == decoded);
         if (rwebm_video_stream_waiting(s))
         {
            if (s->pending)
               CHECK(rwebm_video_stream_consumed(s) <= s->pending_floor);
            rwebm_video_stream_set_avail(s, used);
         }
      }
      CHECK(tries < 100 && position == (targets[i] == 60000000 ? 80000 : targets[i] / 1000));
      CHECK(s->hw && !s->vp9 && !s->frame);
      for (tries = 0; tries < 100; tries++)
         if (rwebm_video_stream_next(s, NULL)) break;
      CHECK(tries < 100 && rwebm_video_stream_hw_frame(s, &frame));
      CHECK(exports == before + 1 && rwebm_video_stream_frame_time_ns(s) == position * 1000);
      if (!i) held = frame;
      else video_hw_frame_abandon(&frame);
   }
   CHECK(rwebm_video_stream_seek_begin_ns(s, 80000000));
   for (tries = 0; tries < 100 && !s->pending; tries++)
      CHECK(rwebm_video_stream_seek_step(s, &position) == 0);
   CHECK(s->pending && rwebm_video_stream_seek_begin_ns(s, 0));
   for (tries = 0; tries < 100; tries++)
      if (rwebm_video_stream_seek_step(s, &position) == 1) break;
   CHECK(tries < 100 && position == 0 && held.image);
   CHECK(!reuse || opens == started + 1);
   rwebm_video_stream_close(s);
   CHECK(lives == 1);
   video_hw_frame_abandon(&held);
   CHECK(!lives && opens == closes && abandoned == exports);
   rvp9_set_hw_backend(&backend);
   return 0;
}

static int packet_stream(void)
{
   static const rvp9_hw_backend_t packets = {
      NULL, open_backend, close_backend, true, NULL, NULL, NULL, NULL
   };
   rwebm_video_stream_t *s;
   video_hw_frame_t frame, held;
   unsigned before = decodes, submitted = calls, shown = 0, tries;
   tail_hidden = 0;
   fixture(1, 2, 1);
   rvp9_set_hw_backend(&packets);
   s = rwebm_video_stream_open(file, used);
   CHECK(s && rwebm_video_stream_set_hw(s, 1) && s->hw->packet_decoder);
   memset(&held, 0, sizeof(held));
   for (tries = 0; tries < 64 && shown < 4; tries++)
   {
      if (!rwebm_video_stream_next(s, NULL))
      {
         CHECK(rwebm_video_stream_waiting(s));
         continue;
      }
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      if (!shown) held = frame;
      else video_hw_frame_abandon(&frame);
      shown++;
   }
   CHECK(shown == 4 && decodes - before == 6 && calls - submitted == 12);
   rwebm_video_stream_close(s);
   CHECK(lives == 1 && held.image);
   video_hw_frame_abandon(&held);
   CHECK(!lives && opens == closes && abandoned == exports);
   rvp9_set_hw_backend(NULL);
   return 0;
}

static int drain_backend(void *owner, bool discard)
{
   mock_t *m = (mock_t *)owner;
   if (m->native_count != terminal_packets)
      return -1;
   m->eos_attempts++;
   if (m->eos_attempts == 1)
      return 1;
   m->eos_accepted = 1;
   return discard || m->shown == terminal_shown ? 0 : 1;
}

static int terminal_stream(void)
{
   static const rvp9_hw_backend_t terminal = {
      NULL, open_backend, close_backend, true, NULL, drain_backend, NULL, NULL
   };
   rwebm_video_stream_t *s;
   video_hw_frame_t frame, held;
   mock_t *m;
   rvp9_hw_backend_t invalid = terminal, snapshot;
   rvp9_hw_ops_t ops;
   rvp9_hdr header;
   int64_t position;
   unsigned tries, shown = 0, before;
   int need_more, status;
   memset(&header, 0, sizeof(header));
   invalid.packet_decoder = false;
   rvp9_set_hw_backend(&invalid);
   CHECK(!rvp9_hw_backend_open(&header, 10, &ops, &snapshot) && !ops.decoder);
   terminal_mode = tail_hidden = 1;
   fixture(1, 2, 1);
   rvp9_set_hw_backend(&terminal);
   s = rwebm_video_stream_open_avail(file, used, prefix, &need_more);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   m = (mock_t *)s->hw->ops.decoder;
   memset(&held, 0, sizeof(held));
   for (tries = 0; tries < 100; tries++)
   {
      if (!rwebm_video_stream_next(s, NULL))
      {
         if (!rwebm_video_stream_waiting(s))
            break;
         if (s->avail < used)
         {
            CHECK(!m->eos_attempts);
            rwebm_video_stream_set_avail(s, used);
         }
         continue;
      }
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      if (!shown)
         held = frame;
      else
         video_hw_frame_abandon(&frame);
      shown++;
   }
   CHECK(tries < 100 && shown == 3 && m->eos_accepted && m->native_count == 7);
   rwebm_video_stream_rewind(s);
   CHECK(lives == 2 && held.image);
   for (tries = 0; tries < 100; tries++)
      if (rwebm_video_stream_next(s, NULL))
         break;
   CHECK(tries < 100 && rwebm_video_stream_hw_frame(s, &frame));
   video_hw_frame_abandon(&frame);
   rwebm_video_stream_close(s);
   CHECK(lives == 1);
   video_hw_frame_abandon(&held);
   CHECK(!lives);
   s = rwebm_video_stream_open(file, used);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   before = exports;
   for (tries = 0; tries < 100; tries++)
   {
      status = rwebm_video_stream_skip(s, NULL);
      if (!status)
         break;
   }
   m = (mock_t *)s->hw->ops.decoder;
   CHECK(tries < 100 && m->eos_accepted && exports == before);
   terminal_shown   = 1;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 80000000));
   for (tries = 0; tries < 100; tries++)
      if (rwebm_video_stream_seek_step(s, &position) == 1)
         break;
   CHECK(tries < 100 && position == 80000);
   for (tries = 0; tries < 100; tries++)
      if (rwebm_video_stream_next(s, NULL))
         break;
   CHECK(tries < 100 && rwebm_video_stream_hw_frame(s, &frame));
   video_hw_frame_abandon(&frame);
   for (tries = 0; tries < 100; tries++)
   {
      CHECK(!rwebm_video_stream_next(s, NULL));
      if (!rwebm_video_stream_waiting(s))
         break;
   }
   CHECK(tries < 100);
   rwebm_video_stream_close(s);
   CHECK(!lives && opens == closes && abandoned == exports);
   terminal_packets = 7;
   terminal_shown   = 3;
   terminal_mode = tail_hidden = 0;
   rvp9_set_hw_backend(NULL);
   return 0;
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
   CHECK(rwebm_video_stream_frame_time_ns(s) == -1);
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
         if (s->timing_active && !s->timing_ready && s->avail < used)
         {
            size_t lo, hi;
            /* A held packet can need the next packet timestamp before
             * decode. Feed the next header without dropping its pin. */
            rwebm_video_stream_read_span(s, &lo, &hi);
            CHECK(frames == 1 && s->pending);
            /* At an exact prefix boundary the next header is absent,
             * so no complete element span has been declared yet. */
            CHECK(!lo && !hi && rwebm_tell(s->timing) == s->avail);
            CHECK(calls == 6 && decodes == 3 && exports == 1);
            rwebm_video_stream_set_avail(s, used);
         }
         continue;
      }
      CHECK(token == &s->hw_marker && !s->frame);
      CHECK(rwebm_video_stream_frame_time_ns(s)
            == (int64_t)(frames < 2 ? frames : 2) * 40000000);
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
   CHECK(rwebm_video_stream_frame_time_ns(s) == -1);
   for (tries = 0; tries < 30; tries++)
      if (rwebm_video_stream_next(s, NULL)) break;
   CHECK(tries < 30 && s->hw_untaken && held.image);
   CHECK(image_transfer_anim_stream_stop_hw(s, IMAGE_TYPE_WEBM));
   CHECK(!s->hw && s->vp9 && !s->hw_want && !s->pkt_idx && closes == 2 && lives == 1);
   /* Old exported output survives a fresh software stream epoch. */
   CHECK(!image_transfer_anim_stream_set_hw(s, IMAGE_TYPE_WEBM, false) && opens == 2);
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
      CHECK(!rwebm_video_stream_next(s, NULL));
      CHECK(rwebm_video_stream_waiting(s) && s->pending
            && s->timing_active && !s->timing_ready);
      CHECK(exports == before && decodes == decoded + 3);
      rwebm_video_stream_set_avail(s, used);
      for (tries = 0; tries < 30; tries++)
         if (rwebm_video_stream_next(s, NULL)) break;
      CHECK(tries < 30 && rwebm_video_stream_hw_frame(s, &frame));
      CHECK(exports == before + 1 && decodes == decoded + 3);
      held = frame;
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
   CHECK(!seek_hardware(0));
   CHECK(!seek_hardware(1));
   rvp9_set_hw_backend(NULL);
   CHECK(!packet_stream());
   CHECK(!color_admission());
   CHECK(!terminal_stream());
   CHECK(!config_admission());
   puts("WebM hardware dispatch, seeks, superframe retries, lacing, partial input and leases passed");
   return 0;
}

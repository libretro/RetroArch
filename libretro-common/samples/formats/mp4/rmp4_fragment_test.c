/* formats/mp4/rmp4.c's movie fragments (moof/traf/trun), against
 * fragmented MP4s built here byte by byte:
 *
 *  - valid: two samples addressed through a trun data offset come
 *    back as packets holding exactly their bytes;
 *  - wrap: a tfhd base-data-offset near 2^64 whose sample end wraps
 *    round to inside the file gives no packet pointing outside it;
 *  - countless: a trun declaring 2^32-1 samples of no size, with no
 *    per-sample fields, opens at once rather than walking them all.
 *
 * Plain C and the C library only. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <formats/rmp4.h>

#ifdef RMP4_TEST_ALLOCATOR
static unsigned fail_next_malloc;
static unsigned fail_next_realloc;
static void *rmp4_test_malloc(size_t size)
{
   if (fail_next_malloc)
   {
      fail_next_malloc = 0;
      return NULL;
   }
   return malloc(size);
}
static void *rmp4_test_realloc(void *ptr, size_t size)
{
   if (fail_next_realloc)
   {
      fail_next_realloc = 0;
      return NULL;
   }
   return realloc(ptr, size);
}
#define malloc rmp4_test_malloc
#define realloc rmp4_test_realloc
#include "../../../formats/mp4/rmp4.c"
#undef malloc
#undef realloc
#endif

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- a box writer ---- */
typedef struct { uint8_t b[4096]; size_t n; } buf_t;

static void put8(buf_t *w, unsigned v)  { w->b[w->n++] = (uint8_t)v; }
static void put32(buf_t *w, uint32_t v)
{ put8(w, v >> 24); put8(w, v >> 16); put8(w, v >> 8); put8(w, v); }
static void put64(buf_t *w, uint64_t v)
{ put32(w, (uint32_t)(v >> 32)); put32(w, (uint32_t)v); }
static void putz(buf_t *w, size_t n) { while (n--) put8(w, 0); }
static void put4cc(buf_t *w, const char *s)
{ put8(w, s[0]); put8(w, s[1]); put8(w, s[2]); put8(w, s[3]); }

/* Opens a box; box_end() writes its size */
static size_t box(buf_t *w, const char *type)
{ size_t at = w->n; put32(w, 0); put4cc(w, type); return at; }
static void box_end(buf_t *w, size_t at)
{
   uint32_t sz = (uint32_t)(w->n - at);
   w->b[at] = sz >> 24; w->b[at + 1] = sz >> 16;
   w->b[at + 2] = sz >> 8; w->b[at + 3] = sz;
}

/* ftyp + moov for one audio track (ID 1, timescale 1000) with an empty
 * sample table and trex defaults of @dflt_size bytes, @dflt_dur ticks */
static void head_private(buf_t *w, uint32_t dflt_size, uint32_t dflt_dur,
      const char *sample, const char *config, size_t *config_pos)
{
   size_t moov, trak, mdia, minf, stbl, b, mvex;
   b = box(w, "ftyp"); put4cc(w, "iso6"); put32(w, 0); put4cc(w, "iso6");
   box_end(w, b);
   moov = box(w, "moov");
   b = box(w, "mvhd"); putz(w, 12); put32(w, 1000); put32(w, 0); putz(w, 80);
   box_end(w, b);
   trak = box(w, "trak");
   b = box(w, "tkhd"); putz(w, 12); put32(w, 1); putz(w, 68); box_end(w, b);
   mdia = box(w, "mdia");
   b = box(w, "mdhd"); putz(w, 12); put32(w, 1000); putz(w, 8); box_end(w, b);
   b = box(w, "hdlr"); putz(w, 8); put4cc(w, sample && !strcmp(sample, "tx3g") ? "sbtl" : sample ? "vide" : "soun"); putz(w, 13);
   box_end(w, b);
   minf = box(w, "minf");
   stbl = box(w, "stbl");
   b = box(w, "stsd"); put32(w, 0); put32(w, sample ? 1 : 0);
   if (sample)
   {
      size_t entry = box(w, sample), child;
      putz(w, 78);
      child = box(w, config);
      *config_pos = w->n;
      put32(w, 0x01020304);
      box_end(w, child); box_end(w, entry);
   }
   box_end(w, b);
   box_end(w, stbl); box_end(w, minf); box_end(w, mdia); box_end(w, trak);
   mvex = box(w, "mvex");
   b = box(w, "trex"); putz(w, 4); put32(w, 1); put32(w, 1);
   put32(w, dflt_dur); put32(w, dflt_size); put32(w, 0); box_end(w, b);
   box_end(w, mvex);
   box_end(w, moov);
}

static void head(buf_t *w, uint32_t dflt_size, uint32_t dflt_dur)
{
   head_private(w, dflt_size, dflt_dur, NULL, NULL, NULL);
}

static void test_long_fragment(void)
{
   static buf_t w;
   const uint32_t count = 262146;
   uint32_t stored = 0, offset;
   size_t moof, traf, b, data_offset, length;
   uint8_t *bytes;
   rmp4_t *m;
   rmp4_packet packet;
   memset(&w, 0, sizeof(w));
   head(&w, 1, 1);
   moof = box(&w, "moof");
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 1); put32(&w, count);
   data_offset = w.n; put32(&w, 0); box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);
   put32(&w, count + 8); put4cc(&w, "mdat");
   offset = (uint32_t)(w.n - moof);
   w.b[data_offset] = offset >> 24; w.b[data_offset + 1] = offset >> 16;
   w.b[data_offset + 2] = offset >> 8; w.b[data_offset + 3] = offset;
   length = w.n + count;
   bytes = (uint8_t*)calloc(length, 1);
   CHECK(bytes != NULL, "long fragment: fixture allocation");
   if (!bytes) return;
   memcpy(bytes, w.b, w.n);
   bytes[length - 1] = 73;
   m = rmp4_open_memory(bytes, length);
   CHECK(m != NULL, "long fragment: opens");
   if (m)
   {
      CHECK(rmp4_track_pts(m, 0, &stored) && stored == count,
            "long fragment: retain all samples, got %u", stored);
      CHECK(rmp4_select_track_sample(m, 0, count - 1)
            && rmp4_read_packet(m, &packet) == 1 && packet.size == 1
            && packet.data[0] == 73, "long fragment: tail seek and read");
      rmp4_close(m);
   }
#ifdef RMP4_TEST_ALLOCATOR
   fail_next_realloc = 1;
   m = rmp4_open_memory(bytes, length);
   CHECK(!fail_next_realloc && !m,
         "long fragment: allocation failure cannot publish a truncated movie");
   rmp4_close(m);
#endif
   free(bytes);
}

/* A released metadata page can disappear while decoder configuration must
 * remain usable by a later seek or backend replacement. */
static void test_private(const char *sample, const char *config,
      enum rmp4_codec codec)
{
   static buf_t w;
   static const uint8_t expected[] = { 1, 2, 3, 4 };
   size_t config_pos = 0;
   rmp4_t *m;
   const rmp4_track *track;
   memset(&w, 0, sizeof(w));
   head_private(&w, 0, 10, sample, config, &config_pos);
   m = rmp4_open_memory(w.b, w.n);
   CHECK(m != NULL, "%s: metadata fixture opens", sample);
   if (!m) return;
   track = rmp4_get_track(m, 0);
   CHECK(track && track->codec == codec, "%s: codec recognised", sample);
   CHECK(track && track->codec_private_size == sizeof(expected),
         "%s: configuration length", sample);
   memset(w.b + config_pos, 0, sizeof(expected));
   rmp4_rewind(m);
   if (track && track->codec_private_size == sizeof(expected))
      CHECK(!memcmp(track->codec_private, expected, sizeof(expected)),
            "%s: decoder configuration survives released input", sample);
   rmp4_close(m);
#ifdef RMP4_TEST_ALLOCATOR
   memset(&w, 0, sizeof(w));
   head_private(&w, 0, 10, sample, config, &config_pos);
   fail_next_malloc = 1;
   m = rmp4_open_memory(w.b, w.n);
   CHECK(!fail_next_malloc, "%s: configuration allocation failure injected", sample);
   CHECK(!m || !rmp4_num_tracks(m), "%s: allocation failure rejects track", sample);
   rmp4_close(m);
#endif
}

/* Every packet the demuxer gives, checked to lie inside the file */
static int read_all(rmp4_t *m, const buf_t *w, const char *name,
      const uint8_t **first)
{
   rmp4_packet pkt;
   int n = 0, r;
   for (;;)
   {
      size_t lo = 17, hi = 19;
      size_t again_lo = 23, again_hi = 29;
      rmp4_read_span(m, &lo, &hi);
      rmp4_read_span(m, &again_lo, &again_hi);
      CHECK(lo == again_lo && hi == again_hi,
            "%s: range query leaves the cursor intact", name);
      r = rmp4_read_packet(m, &pkt);
      if (r != 1)
         break;
      if (hi > lo)
         CHECK(lo == (size_t)(pkt.data - w->b) && hi - lo == pkt.size,
               "%s: predicted range matches packet %d", name, n);
      CHECK(pkt.data >= w->b && pkt.size <= w->n
            && (size_t)(pkt.data - w->b) <= w->n - pkt.size,
            "%s: packet %d points outside the file", name, n);
      if (!n && first)
         *first = pkt.data;
      if (++n > 16)
         break;
   }
   return n;
}

static void test_sample_cursor(rmp4_t *m, const buf_t *w, size_t payload)
{
   rmp4_packet pkt;
   size_t begin = 17, end = 19;
   rmp4_set_avail(m, 0);
   CHECK(rmp4_track_sample_span(m, 0, 1, &begin, &end)
         && begin == payload + 4 && end == payload + 8,
         "sample span: metadata without resident media");
   CHECK(!rmp4_track_sample_span(m, -1, 0, &begin, &end)
         && !rmp4_track_sample_span(m, 0, 2, &begin, &end)
         && !rmp4_track_sample_span(m, 0, 0, NULL, &end)
         && begin == payload + 4 && end == payload + 8,
         "sample span: invalid queries preserve outputs");
   CHECK(rmp4_track_sample_key(m, 0, 0) == 1,
         "sample cursor: key metadata without resident media");
   CHECK(rmp4_track_sample_key(m, -1, 0) == -1
         && rmp4_track_sample_key(m, 0, 2) == -1,
         "sample cursor: invalid key query");
   CHECK(rmp4_select_track_sample(m, 0, 1), "sample cursor: select second");
   CHECK(rmp4_consumed(m) == payload + 4,
         "sample cursor: feeder anchor follows selected sample");
   rmp4_read_span(m, &begin, &end);
   CHECK(begin == payload + 4 && end == payload + 8,
         "next packet range: metadata without resident media");
   CHECK(rmp4_read_packet(m, &pkt) == RMP4_READ_AGAIN,
         "sample cursor: read respects byte wall");
   rmp4_read_span(m, NULL, &end);
   rmp4_read_span(m, &begin, NULL);
   CHECK(begin == payload + 4 && end == payload + 8
         && rmp4_consumed(m) == payload + 4,
         "next packet range: retry and nullable outputs preserve cursor");
   CHECK(!rmp4_select_track_sample(m, -1, 0)
         && !rmp4_select_track_sample(m, 0, 2)
         && rmp4_consumed(m) == payload + 4,
         "sample cursor: invalid selection leaves position intact");
   rmp4_set_avail(m, w->n);
   CHECK(rmp4_read_packet(m, &pkt) == 1
         && pkt.data == w->b + payload + 4 && pkt.size == 4,
         "sample cursor: retry reads the selected sample");
   CHECK(rmp4_read_packet(m, &pkt) == 0, "sample cursor: selected track ends");
   rmp4_read_span(m, &begin, &end);
   CHECK(!begin && !end, "next packet range: EOF clears outputs");
   rmp4_rewind(m);
   CHECK(rmp4_consumed(m) == payload
         && rmp4_read_packet(m, &pkt) == 1 && pkt.data == w->b + payload,
         "sample cursor: rewind restores beginning");
   rmp4_read_span(m, &begin, &end);
   CHECK(begin == payload + 4 && end == payload + 8,
         "next packet range: follows rewound cursor");
#ifdef RMP4_TEST_ALLOCATOR
   {
      rmp4_itrack *track = &m->trk[0];
      uint32_t cursor = track->cursor;
      uint64_t offset = track->off[cursor];
      uint32_t size = track->size[cursor];
      track->off[cursor] = (uint64_t)m->len + 1;
      rmp4_read_span(m, &begin, &end);
      CHECK(!begin && !end, "next packet range: offset beyond file refused");
      track->off[cursor] = offset;
      track->size[cursor] = UINT32_MAX;
      rmp4_read_span(m, &begin, &end);
      CHECK(!begin && !end, "next packet range: end beyond file refused");
      track->size[cursor] = size;
   }
#endif
   rmp4_read_span(NULL, &begin, &end);
   CHECK(!begin && !end, "next packet range: null stream clears outputs");
}

#ifdef RMP4_TEST_ALLOCATOR
static void test_long_track(void)
{
   static buf_t sizes, times, chunks, offsets;
   static const uint32_t counts[] = {262143, 262144, 262145, 500000};
   rmp4_stbl tables;
   unsigned c;
   memset(&tables, 0, sizeof(tables));
   for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
   {
      uint32_t count = counts[c], stored = 0;
      uint8_t *bytes = (uint8_t*)calloc(count, 1);
      rmp4_t *m = (rmp4_t*)calloc(1, sizeof(*m));
      rmp4_packet packet;
      const int64_t *pts;
      CHECK(bytes && m, "long track: fixture allocation");
      if (!bytes || !m) { free(bytes); free(m); return; }
      sizes.n = times.n = chunks.n = offsets.n = 0;
      put32(&sizes, 0); put32(&sizes, 1); put32(&sizes, count);
      put32(&times, 0); put32(&times, 1);
      put32(&times, count); put32(&times, 1);
      put32(&chunks, 0); put32(&chunks, 1);
      put32(&chunks, 1); put32(&chunks, count); put32(&chunks, 1);
      put32(&offsets, 0); put32(&offsets, 1); put32(&offsets, 0);
      tables.stsz = sizes.b; tables.stsz_size = sizes.n;
      tables.stts = times.b; tables.stts_size = times.n;
      tables.stsc = chunks.b; tables.stsc_size = chunks.n;
      tables.stco = offsets.b; tables.stco_size = offsets.n;
      m->trk[0].timescale = 30;
      m->buf = bytes; m->len = m->avail = count; m->num_tracks = 1;
      CHECK(rmp4_build_samples(&m->trk[0], &tables, count),
            "long track: build %u samples", count);
      pts = rmp4_track_pts(m, 0, &stored);
      CHECK(pts && stored == count, "long track: retain all %u samples, got %u", count, stored);
      bytes[count - 2] = 17; bytes[count - 1] = 29;
      CHECK(rmp4_select_track_sample(m, 0, count - 2),
            "long track: seek past old cutoff");
      CHECK(rmp4_read_packet(m, &packet) == 1 && packet.size == 1
            && packet.data[0] == 17, "long track: first tail sample");
      CHECK(rmp4_read_packet(m, &packet) == 1 && packet.size == 1
            && packet.data[0] == 29, "long track: playback continues after seek");
      CHECK(rmp4_read_packet(m, &packet) == 0, "long track: actual end of track");
      rmp4_close(m);
      free(bytes);
   }
   {
      rmp4_itrack *track = (rmp4_itrack*)calloc(1, sizeof(*track));
      CHECK(track != NULL, "index budget: fixture allocation");
      if (!track) return;
      sizes.n = 0;
      put32(&sizes, 0); put32(&sizes, 1); put32(&sizes, RMP4_MAX_SAMPLES + 1);
      fail_next_malloc = 1;
      CHECK(!rmp4_build_samples(track, &tables, RMP4_MAX_SAMPLES + 1)
            && !track->count && !track->off && fail_next_malloc,
            "index budget: reject excess before allocation instead of truncating");
      fail_next_malloc = 0;
      free(track);
   }
}

static void test_text_timing(void)
{
   static buf_t sizes, times, chunks, offsets;
   rmp4_stbl tables;
   rmp4_t *m = (rmp4_t*)calloc(1, sizeof(*m));
   unsigned i;
   int64_t first = -9, last = -7;
   CHECK(m != NULL, "text timing: allocation");
   if (!m) return;
   memset(&tables, 0, sizeof(tables));
   sizes.n = times.n = chunks.n = offsets.n = 0;
   put32(&sizes, 0); put32(&sizes, 1); put32(&sizes, 3);
   put32(&times, 0); put32(&times, 3);
   put32(&times, 1); put32(&times, 1501);
   put32(&times, 1); put32(&times, 0);
   put32(&times, 1); put32(&times, 2999);
   put32(&chunks, 0); put32(&chunks, 1);
   put32(&chunks, 1); put32(&chunks, 3); put32(&chunks, 1);
   put32(&offsets, 0); put32(&offsets, 1); put32(&offsets, 0);
   tables.stsz = sizes.b; tables.stsz_size = sizes.n;
   tables.stts = times.b; tables.stts_size = times.n;
   tables.stsc = chunks.b; tables.stsc_size = chunks.n;
   tables.stco = offsets.b; tables.stco_size = offsets.n;
   m->num_tracks = 1; m->len = 2048;
   m->trk[0].timescale = 3000;
   m->trk[0].pub.type = RMP4_TRACK_OTHER;
   m->trk[0].pub.presentation_duration_ns = -1;
   CHECK(rmp4_build_samples(&m->trk[0], &tables, m->len), "text timing: stts");
   CHECK(rmp4_track_text_duration(m, 0, 0) == 500333333,
         "text timing: exact rational duration");
   CHECK(rmp4_track_text_duration(m, 0, 1) == 0,
         "text timing: zero duration is retained");
   CHECK(rmp4_track_text_duration(m, 0, 2) == 999666666,
         "text timing: final sample needs no following PTS");
   CHECK(rmp4_track_text_duration(m, -1, 0) == -1
         && rmp4_track_text_duration(m, 0, 3) == -1
         && rmp4_track_text_duration(NULL, 0, 0) == -1,
         "text timing: invalid queries refused");
   CHECK(rmp4_track_text_time(m, 0, 0, &first, &last)
         && first == 0 && last == 500333333,
         "text timing: unedited movie interval");
   m->trk[0].pub.media_skip = 1500;
   m->trk[0].pub.presentation_delay_ns = 250000000;
   m->trk[0].pub.presentation_duration_ns = 700000000;
   CHECK(rmp4_track_text_time(m, 0, 0, &first, &last)
         && first == 250000000 && last == 250333333,
         "text timing: trim across first cue and leading delay");
   CHECK(rmp4_track_text_time(m, 0, 2, &first, &last)
         && first == 250333333 && last == 950000000,
         "text timing: cue clipped to edit end");
   m->trk[0].pts[0] = (int64_t)((~(uint64_t)0) >> 1) - 1;
   first = -9; last = -7;
   CHECK(!rmp4_track_text_time(m, 0, 0, &first, &last) && first == -9 && last == -7,
         "text timing: unrepresentable endpoint leaves outputs intact");
   for (i = 0; i < 1024; i++)
      CHECK(rmp4_track_append(&m->trk[0], 3 + i, 1,
               1500000000 + (int64_t)i * 1000000, i + 1, 1),
            "text timing: fragment append %u", i);
   CHECK(rmp4_track_text_duration(m, 0, 1026) == 341333333,
         "text timing: fragment duration survives array growth");
   rmp4_close(m);
   m = (rmp4_t*)calloc(1, sizeof(*m));
   CHECK(m != NULL, "text timing: audio allocation");
   if (!m) return;
   m->num_tracks = 1; m->len = 2048;
   m->trk[0].timescale = 3000;
   m->trk[0].pub.type = RMP4_TRACK_AUDIO;
   CHECK(rmp4_build_samples(&m->trk[0], &tables, m->len)
         && !m->trk[0].duration && rmp4_track_text_duration(m, 0, 0) == -1,
         "text timing: no extra table for audio/video");
   rmp4_close(m);
}

static void test_track_selection(void)
{
   static const uint8_t bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
   uint64_t offsets0[2] = {0, 4}, offsets1[2] = {2, 6};
   uint32_t sizes[2] = {2, 2};
   int64_t pts[2] = {0, 1000000};
   uint8_t keys[2] = {1, 0};
   rmp4_t *m = (rmp4_t*)calloc(1, sizeof(*m));
   rmp4_packet packet;
   int i;
   size_t begin = 17, end = 19;
   CHECK(m != NULL, "track selection: allocation");
   if (!m) return;
   m->buf = bytes;
   m->len = m->avail = sizeof(bytes);
   m->num_tracks = 2;
   for (i = 0; i < 2; i++)
   {
      m->trk[i].off = i ? offsets1 : offsets0;
      m->trk[i].size = sizes;
      m->trk[i].pts = pts;
      m->trk[i].key = keys;
      m->trk[i].count = 2;
   }
   CHECK(rmp4_track_sample_key(m, 0, 1) == 0,
         "track selection: non-key metadata");
   CHECK(rmp4_select_track_sample(m, 0, 1), "track selection: select second sample");
   rmp4_read_span(m, &begin, &end);
   CHECK(begin == 4 && end == 6,
         "next packet range: selected track parks the other cursor");
   CHECK(rmp4_read_packet(m, &packet) == 1
         && packet.track == 0 && packet.data == bytes + 4
         && rmp4_read_packet(m, &packet) == 0,
         "track selection: other track parked at end");
   rmp4_rewind(m);
   for (i = 0; i < 4; i++)
   {
      rmp4_read_span(m, &begin, &end);
      CHECK(begin == (size_t)i * 2 && end == (size_t)i * 2 + 2,
            "next packet range: interleaved tracks follow file order");
      CHECK(rmp4_read_packet(m, &packet) == 1
            && packet.track == (i & 1) && packet.data == bytes + i * 2,
            "track selection: rewind restores both tracks");
   }
   rmp4_read_span(m, &begin, &end);
   CHECK(!begin && !end, "next packet range: all tracks exhausted");
   rmp4_rewind(m);
   begin = 17;
   end = 19;
   offsets0[1] = sizeof(bytes) - 1;
   CHECK(!rmp4_track_sample_span(m, 0, 1, &begin, &end)
         && begin == 17 && end == 19,
         "sample span: rejects range beyond file");
   offsets0[1] = (uint64_t)-1;
   CHECK(!rmp4_track_sample_span(m, 0, 1, &begin, &end)
         && begin == 17 && end == 19,
         "sample span: rejects wrapped offset");
   CHECK(!rmp4_select_track_sample(m, 0, 1)
         && rmp4_read_packet(m, &packet) == 1 && packet.data == bytes,
         "track selection: malformed offset leaves cursors unchanged");
   free(m);
}
#endif

static void test_text_fragment(void)
{
   static buf_t w;
   size_t unused, moof, traf, b, data_offset, payload;
   rmp4_t *m;
   uint32_t delta;
   memset(&w, 0, sizeof(w));
   head_private(&w, 4, 17, "tx3g", "free", &unused);
   moof = box(&w, "moof");
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0x000101); put32(&w, 2);
   data_offset = w.n; put32(&w, 0);
   put32(&w, 1201); put32(&w, 701);
   box_end(&w, b); box_end(&w, traf); box_end(&w, moof);
   b = box(&w, "mdat"); payload = w.n;
   put32(&w, 0x00026869); put32(&w, 0x00026f6b); box_end(&w, b);
   delta = (uint32_t)(payload - moof);
   w.b[data_offset] = delta >> 24; w.b[data_offset + 1] = delta >> 16;
   w.b[data_offset + 2] = delta >> 8; w.b[data_offset + 3] = delta;
   m = rmp4_open_memory(w.b, w.n);
   CHECK(m && rmp4_num_tracks(m) == 1, "text fragment: opens");
   if (!m) return;
   CHECK(rmp4_get_track(m, 0)->type == RMP4_TRACK_OTHER
         && !strcmp(rmp4_get_track(m, 0)->codec_id, "tx3g"),
         "text fragment: codec identity");
   CHECK(rmp4_track_text_duration(m, 0, 0) == 1201000000
         && rmp4_track_text_duration(m, 0, 1) == 701000000,
         "text fragment: per-sample duration overrides trex");
   memset(w.b, 0, w.n);
   CHECK(rmp4_track_text_duration(m, 0, 1) == 701000000,
         "text fragment: duration survives released encoded metadata");
   rmp4_close(m);
}

static void test_valid(void)
{
   buf_t w;
   size_t moof, traf, b, trun_doff, mdat_payload;
   const uint8_t *first = NULL;
   rmp4_t *m;
   int n;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 10);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0x000201); put32(&w, 2);
   trun_doff = w.n; put32(&w, 0);              /* data offset, below */
   put32(&w, 4); put32(&w, 4);                 /* sample sizes       */
   box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);
   b = box(&w, "mdat");
   mdat_payload = w.n;
   put32(&w, 0x11223344); put32(&w, 0x55667788);
   box_end(&w, b);
   /* the offset is from the moof's start */
   {
      uint32_t d = (uint32_t)(mdat_payload - moof);
      w.b[trun_doff] = d >> 24; w.b[trun_doff + 1] = d >> 16;
      w.b[trun_doff + 2] = d >> 8; w.b[trun_doff + 3] = d;
   }

   m = rmp4_open_memory(w.b, w.n);
   CHECK(m != NULL, "valid: opens");
   if (!m)
      return;
   CHECK(rmp4_get_track(m, 0)->presentation_duration_ns == -1,
         "valid: absent edit duration remains unknown");
   n = read_all(m, &w, "valid", &first);
   CHECK(n == 2, "valid: %d packets, not 2", n);
   CHECK(first == w.b + mdat_payload, "valid: the first packet is the mdat's");
   test_sample_cursor(m, &w, mdat_payload);
   rmp4_close(m);
   printf("valid: ok\n");
}

static void test_wrap(void)
{
   buf_t w;
   size_t moof, traf, b;
   rmp4_t *m;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 10);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   /* base-data-offset 2^64 - 16: plus a 32-byte sample, the end wraps
    * to 16, inside the file */
   b = box(&w, "tfhd"); put32(&w, 0x000001); put32(&w, 1);
   put64(&w, (uint64_t)0 - 16); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0x000200); put32(&w, 1); put32(&w, 32);
   box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);
   b = box(&w, "mdat"); putz(&w, 64); box_end(&w, b);

   m = rmp4_open_memory(w.b, w.n);
   if (m)
   {
      read_all(m, &w, "wrap", NULL);
      rmp4_close(m);
   }
   printf("wrap: ok\n");
}

static void test_countless(void)
{
   buf_t w;
   size_t moof, traf, b;
   clock_t t0;
   double secs;
   rmp4_t *m;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 1);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0); put32(&w, 0xFFFFFFFFu); box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);

   t0   = clock();
   m    = rmp4_open_memory(w.b, w.n);
   secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
   CHECK(secs < 1.0, "countless: open took %.1f s", secs);
   if (m)
   {
      read_all(m, &w, "countless", NULL);
      rmp4_close(m);
   }
   printf("countless: ok (%.3f s)\n", secs);
}

#ifdef RMP4_TEST_ALLOCATOR
static void edit_entry(buf_t *w, unsigned version, uint64_t duration,
      uint64_t media, uint32_t rate)
{
   if (version) { put64(w, duration); put64(w, media); }
   else { put32(w, (uint32_t)duration); put32(w, (uint32_t)media); }
   put32(w, rate);
}

/* Construct actual box bytes, including child order and malformed sizes;
 * expectations are track trim/delay values, not a replay of parser logic. */
static void test_edit_lists(void)
{
   static buf_t w, edit;
   unsigned version, mutation;
   for (version = 0; version <= 1; version++)
      for (mutation = 0; mutation < 15; mutation++)
      {
         rmp4_box moov, trak, mvhd;
         size_t moov_at, trak_at, insert, outer, inner;
         uint32_t count = mutation == 1 ? 1 : mutation == 6 ? 3 : 2;
         uint64_t delay = mutation == 9 ? ((~(uint64_t)0) >> 1) : 1500;
         uint64_t media = mutation == 5 ? (uint64_t)-2 : 128;
         uint64_t play = mutation == 12 ? ((~(uint64_t)0) >> 1)
            : mutation == 13 ? 0 : mutation == 14 ? ~(uint64_t)0 : 1000;
         int valid = mutation == 0 || mutation == 1 || mutation == 7
            || mutation == 13 || ((mutation == 9 || mutation == 12
                     || mutation == 14) && !version);
         int parsed;
         rmp4_t *m;
         memset(&w, 0, sizeof(w)); head(&w, 4, 40);
         CHECK(rmp4_find_child(w.b, w.n, RMP4_FOURCC('m','o','o','v'), &moov), "edit moov");
         CHECK(rmp4_find_child(moov.body, moov.size, RMP4_FOURCC('t','r','a','k'), &trak), "edit trak");
         moov_at = (size_t)(moov.body - w.b) - 8;
         trak_at = (size_t)(trak.body - w.b) - 8;
         insert = (size_t)(trak.body - w.b) + (size_t)trak.size;
         memset(&edit, 0, sizeof(edit));
         outer = box(&edit, "edts"); inner = box(&edit, "elst");
         put32(&edit, (mutation == 2 ? 2 : version) << 24); put32(&edit, count);
         if (count > 1)
            edit_entry(&edit, version, delay, mutation == 10 ? 0 : (uint64_t)-1,
                  mutation == 11 ? 0x00018000u : 0x00010000u);
         if (count > 2) edit_entry(&edit, version, 500, 0, 0x00010000u);
         edit_entry(&edit, version, play, media, mutation == 3 ? 0 : 0x00010000u);
         box_end(&edit, inner); box_end(&edit, outer);
         if (mutation == 4) edit.n--;
         memmove(w.b + insert + edit.n, w.b + insert, w.n - insert);
         memcpy(w.b + insert, edit.b, edit.n); w.n += edit.n;
         /* Patch just the two enclosing sizes. */
         {
            size_t end = w.n;
            w.n = insert + edit.n; box_end(&w, trak_at);
            w.n = end; box_end(&w, moov_at);
         }
         CHECK(rmp4_find_child(w.b, w.n, RMP4_FOURCC('m','o','o','v'), &moov), "edited moov");
         CHECK(rmp4_find_child(moov.body, moov.size, RMP4_FOURCC('m','v','h','d'), &mvhd), "edit mvhd");
         if (mutation == 8)
         {
            size_t scale = (size_t)(mvhd.body - w.b) + 12;
            memset(w.b + scale, 0, 4);
         }
         if (mutation == 7)
         {
            uint8_t header[128];
            size_t start = (size_t)(mvhd.body - w.b) - 8;
            size_t n = (size_t)mvhd.size + 8;
            memcpy(header, w.b + start, n);
            memmove(w.b + start, w.b + start + n, w.n - start - n);
            memcpy(w.b + w.n - n, header, n);
         }
         m = (rmp4_t*)calloc(1, sizeof(*m));
         CHECK(m != NULL, "edit allocation");
         if (!m) continue;
         parsed = rmp4_parse_moov(m, w.b + moov_at + 8, w.n - moov_at - 8);
         if (mutation == 8 || ((mutation == 9 || mutation == 12) && version))
            CHECK(!parsed, "edit v%u mutation%u must reject unrepresentable delay", version, mutation);
         else
         {
            CHECK(parsed && m->num_tracks == 1, "edit v%u mutation%u parse", version, mutation);
            CHECK(m->trk[0].pub.media_skip == (valid ? 128u : 0u),
                  "edit v%u mutation%u trim", version, mutation);
            CHECK(m->trk[0].pub.presentation_delay_ns == (mutation == 9 && !version
                     ? (int64_t)4294967295u * 1000000
                     : valid && mutation != 1 ? 1500000000 : 0),
                  "edit v%u mutation%u delay", version, mutation);
            CHECK(m->trk[0].pub.presentation_duration_ns == (!valid ? -1
                     : mutation == 13 ? 0 : mutation == 12 || mutation == 14
                     ? (int64_t)4294967295u * 1000000 : 1000000000),
                  "edit v%u mutation%u play length", version, mutation);
         }
         rmp4_close(m);
      }
   puts("MP4 edit-list v0/v1, bounds, rate, child order and delay overflow cases complete");
}
#endif

int main(void)
{
   test_valid();
   test_text_fragment();
   test_long_fragment();
#ifdef RMP4_TEST_ALLOCATOR
   test_long_track();
   test_track_selection();
   test_text_timing();
   test_edit_lists();
#endif
   test_wrap();
   test_countless();
   test_private("avc1", "avcC", RMP4_CODEC_H264);
   test_private("hvc1", "hvcC", RMP4_CODEC_H265);
   test_private("av01", "av1C", RMP4_CODEC_AV1);
   test_private("vp08", "vpcC", RMP4_CODEC_VP8);
   test_private("vp09", "vpcC", RMP4_CODEC_VP9);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("rmp4_fragment: all passed\n");
   return 0;
}

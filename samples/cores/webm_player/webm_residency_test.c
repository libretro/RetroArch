/* SPDX-License-Identifier: MIT */
#include "../../../cores/libretro-media/media_core.c"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static uint8_t *file;
static size_t used;

static size_t element(unsigned id, unsigned bytes)
{
   size_t at;
   while (bytes--)
      file[used++] = (uint8_t)(id >> (bytes * 8));
   at = used;
   used += 4;
   return at;
}

static void size_vint(size_t at, size_t n)
{
   file[at] = (uint8_t)(0x10 | (n >> 24));
   file[at + 1] = (uint8_t)(n >> 16);
   file[at + 2] = (uint8_t)(n >> 8);
   file[at + 3] = (uint8_t)n;
}

static void finish(size_t at)
{
   size_vint(at, used - at - 4);
}

static void integer(unsigned id, unsigned value)
{
   size_t at = element(id, 1);
   file[used++] = (uint8_t)value;
   finish(at);
}

static int check_lacing(unsigned mode)
{
   media_player_t p;
   rwebm_packet packet;
   size_t segment, tracks, track, video, cluster, block, at;
   size_t first, floor, consumed, n = MEDIA_DISCARD_MARGIN + 4096;
   unsigned i;
   memset(&p, 0, sizeof(p));
   used = 0;
   at = element(0x1a45dfa3, 4); finish(at);
   segment = element(0x18538067, 4);
   tracks = element(0x1654ae6b, 4);
   track = element(0xae, 1);
   integer(0xd7, 1); integer(0x83, 1);
   at = element(0x86, 1);
   memcpy(file + used, "V_VP9", 5); used += 5; finish(at);
   video = element(0xe0, 1);
   integer(0xb0, 64); integer(0xba, 48);
   finish(video); finish(track); finish(tracks);
   cluster = element(0x1f43b675, 4); integer(0xe7, 0);
   block = element(0xa3, 1);
   file[used++] = 0x81;
   file[used++] = 0; file[used++] = 0;
   file[used++] = (uint8_t)(0x80 | (mode << 1));
   file[used++] = 2;
   if (mode == 1)
   {
      for (i = 0; i < 2; i++)
      {
         size_t remain = n;
         while (remain >= 255)
         {
            file[used++] = 255;
            remain -= 255;
         }
         file[used++] = (uint8_t)remain;
      }
   }
   else if (mode == 3)
   {
      size_vint(used, n); used += 4;
      file[used++] = 0xbf; /* second frame has equal size */
   }
   first = used;
   for (i = 0; i < 3; i++)
   {
      memset(file + used, (int)(0x31 + i), n);
      used += n;
   }
   finish(block); finish(cluster); finish(segment);
   p.webm = rwebm_open_memory_avail(file, used, first + n, NULL);
   CHECK(p.webm);
   floor = rwebm_consumed(p.webm);
   {
      size_t lo, hi;
      CHECK(rwebm_read_packet(p.webm, &packet) == RWEBM_READ_AGAIN);
      rwebm_read_span(p.webm, &lo, &hi);
      CHECK(lo == floor && hi == used);
      rwebm_rewind(p.webm);
      rwebm_read_span(p.webm, &lo, &hi);
      CHECK(!lo && !hi);
      CHECK(rwebm_read_packet(p.webm, &packet) == RWEBM_READ_AGAIN);
      rwebm_set_avail(p.webm, used);
   }
   CHECK(rwebm_read_packet(p.webm, &packet) == 1);
   {
      size_t lo, hi;
      rwebm_read_span(p.webm, &lo, &hi);
      CHECK(!lo && !hi);
   }
   CHECK(packet.data == file + first && packet.size == n);
   CHECK(!packet.timestamp_continuation);
   CHECK(rwebm_tell(p.webm) - MEDIA_DISCARD_MARGIN > first + n);
   CHECK(rwebm_consumed(p.webm) == first + n);
   CHECK(media_native_consumed(&p) == first + n);
   p.wgather = rwebm_open_memory(file, used);
   CHECK(p.wgather);
   CHECK(media_native_consumed(&p) == floor);
   CHECK(rwebm_read_packet(p.wgather, &packet) == 1);
   consumed = media_native_consumed(&p);
   CHECK(consumed == first + n);
   /* The release boundary must precede every unread frame. */
   CHECK(consumed - MEDIA_DISCARD_MARGIN <= first + n);
   CHECK(rwebm_read_packet(p.webm, &packet) == 1);
   CHECK(packet.data == file + first + n && packet.data[0] == 0x32);
   CHECK(packet.timestamp_continuation);
   CHECK(media_native_consumed(&p) == consumed);
   CHECK(rwebm_read_packet(p.wgather, &packet) == 1);
   CHECK(media_native_consumed(&p) == first + 2 * n);
   CHECK(rwebm_read_packet(p.webm, &packet) == 1);
   CHECK(packet.data == file + first + 2 * n && packet.data[n - 1] == 0x33);
   CHECK(packet.timestamp_continuation);
   CHECK(media_native_consumed(&p) == first + 2 * n);
   rwebm_close(p.wgather); p.wgather = NULL;
   CHECK(media_native_consumed(&p) == rwebm_tell(p.webm));
   rwebm_rewind(p.webm);
   CHECK(media_native_consumed(&p) == floor);
   rwebm_close(p.webm);
   return 0;
}

struct void_notice { size_t lo, hi; unsigned calls; int refuse; };
static int notice_void(void *user, size_t lo, size_t hi)
{
   struct void_notice *notice = (struct void_notice*)user;
   notice->lo = lo; notice->hi = hi; notice->calls++;
   return !notice->refuse;
}

static int check_void_frontier(void)
{
   rwebm_t *demux, *clone;
   struct void_notice notice;
   rwebm_packet packet;
   size_t at, segment, tracks, track, video, cluster, block, padding;
   size_t prefix, resume;
   used = 0;
   at = element(0x1a45dfa3, 4); finish(at);
   segment = element(0x18538067, 4);
   tracks = element(0x1654ae6b, 4);
   track = element(0xae, 1);
   integer(0xd7, 1); integer(0x83, 1);
   at = element(0x86, 1); memcpy(file + used, "V_VP9", 5); used += 5; finish(at);
   video = element(0xe0, 1);
   integer(0xb0, 64); integer(0xba, 48);
   finish(video); finish(track); finish(tracks);
   cluster = element(0x1f43b675, 4); integer(0xe7, 0);
   block = element(0xa3, 1);
   file[used++] = 0x81; file[used++] = 0; file[used++] = 0;
   file[used++] = 0x80; file[used++] = 0x31; finish(block);
   padding = element(0xec, 1); prefix = used;
   memset(file + used, 0, MEDIA_DISCARD_MARGIN * 2); used += MEDIA_DISCARD_MARGIN * 2;
   finish(padding); resume = used;
   block = element(0xa3, 1);
   file[used++] = 0x81; file[used++] = 0; file[used++] = 1;
   file[used++] = 0x80; file[used++] = 0x32; finish(block);
   finish(cluster); finish(segment);
   demux = rwebm_open_memory_avail(file, used, prefix, NULL);
   CHECK(demux);
   memset(&notice, 0, sizeof(notice)); notice.refuse = 1;
   rwebm_set_inert_callback(demux, notice_void, &notice);
   CHECK(rwebm_read_packet(demux, &packet) == 1 && packet.data[0] == 0x31);
   clone = rwebm_cursor_clone(demux);
   CHECK(clone);
   CHECK(rwebm_read_packet(clone, &packet) == -1);
   CHECK(notice.calls == 1 && notice.lo == prefix && notice.hi == resume);
   rwebm_close(clone);
   notice.refuse = 0;
   CHECK(rwebm_read_packet(demux, &packet) == RWEBM_READ_AGAIN);
   CHECK(notice.calls == 2 && notice.lo == prefix && notice.hi == resume);
   CHECK(rwebm_consumed(demux) == resume);
   rwebm_set_avail(demux, used);
   CHECK(rwebm_read_packet(demux, &packet) == 1
         && packet.data[0] == 0x32 && packet.timestamp == 1000000);
   rwebm_close(demux);
   size_vint(padding, used + 1024);
   demux = rwebm_open_memory(file, used);
   CHECK(demux);
   memset(&notice, 0, sizeof(notice));
   rwebm_set_inert_callback(demux, notice_void, &notice);
   CHECK(rwebm_read_packet(demux, &packet) == 1);
   CHECK(rwebm_read_packet(demux, &packet) == -1);
   CHECK(!notice.calls);
   rwebm_close(demux);
   return 0;
}

int main(void)
{
   int failed = 0;
   unsigned mode;
   file = (uint8_t*)malloc(3 * (MEDIA_DISCARD_MARGIN + 4096) + 65536);
   if (!file)
      return 1;
   for (mode = 1; mode <= 3; mode++)
      failed |= check_lacing(mode);
   failed |= check_void_frontier();
   free(file);
   if (!failed)
      puts("WebM movie residency: Xiph/fixed/EBML laces and partial Void frontier pass");
   return failed;
}

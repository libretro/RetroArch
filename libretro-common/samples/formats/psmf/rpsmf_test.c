/* rpsmf and the MPEG-2 side of rmpeg1_ps against program streams built
 * here: a PSMF header, MPEG-2 packs and PES headers, H.264 video with
 * PTS and DTS, and ATRAC3plus frames running across private stream 1
 * packets behind the packets' own header bytes - fed whole, a byte at a
 * time and in odd chunks, with a broken frame in between. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rpsmf.h>
#include <formats/rmpeg1_ps.h>

static unsigned failures;

#define CHECK(c, msg) do { if (!(c)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)

static uint8_t ps[1 << 16];
static size_t  ps_len;

static void put(const void *p, size_t n)
{
   if (n)
      memcpy(ps + ps_len, p, n);
   ps_len += n;
}

static void put_ts(unsigned prefix, uint64_t ts)
{
   uint8_t b[5];
   b[0] = (uint8_t)((prefix << 4) | (((ts >> 30) & 7) << 1) | 1);
   b[1] = (uint8_t)(ts >> 22);
   b[2] = (uint8_t)((((ts >> 15) & 0x7F) << 1) | 1);
   b[3] = (uint8_t)(ts >> 7);
   b[4] = (uint8_t)(((ts & 0x7F) << 1) | 1);
   put(b, 5);
}

/* An MPEG-2 pack header, SCR @scr, two stuffing bytes */
static void put_pack2(uint64_t scr)
{
   uint8_t b[16];
   uint32_t mux = 0x1234;
   b[0]  = 0; b[1] = 0; b[2] = 1; b[3] = 0xBA;
   b[4]  = (uint8_t)(0x44 | (((scr >> 30) & 7) << 3) | ((scr >> 28) & 3));
   b[5]  = (uint8_t)(scr >> 20);
   b[6]  = (uint8_t)(((scr >> 15) & 0x1F) << 3 | 4 | ((scr >> 13) & 3));
   b[7]  = (uint8_t)(scr >> 5);
   b[8]  = (uint8_t)(((scr & 0x1F) << 3) | 4);
   b[9]  = 1;
   b[10] = (uint8_t)(mux >> 14);
   b[11] = (uint8_t)(mux >> 6);
   b[12] = (uint8_t)(((mux & 0x3F) << 2) | 3);
   b[13] = 0xF8 | 2;
   b[14] = 0xFF;
   b[15] = 0xFF;
   put(b, 16);
}

/* An MPEG-2 PES of @id: PTS (and DTS) when given, @lead bytes before
 * @payload (a private stream's substream byte and header) */
static void put_pes2(uint8_t id, uint64_t pts, uint64_t dts,
      const uint8_t *lead, size_t lead_len,
      const uint8_t *payload, size_t len)
{
   uint8_t b[9];
   size_t hlen  = pts == RPSMF_NO_PTS ? 0 : dts == RPSMF_NO_PTS ? 5 : 10;
   size_t total = 3 + hlen + lead_len + len;
   b[0] = 0; b[1] = 0; b[2] = 1; b[3] = id;
   b[4] = (uint8_t)(total >> 8);
   b[5] = (uint8_t)total;
   b[6] = 0x81;
   b[7] = (uint8_t)(hlen == 10 ? 0xC0 : hlen ? 0x80 : 0);
   b[8] = (uint8_t)hlen;
   put(b, 9);
   if (hlen)
      put_ts(hlen == 10 ? 3 : 2, pts);
   if (hlen == 10)
      put_ts(1, dts);
   put(lead, lead_len);
   put(payload, len);
}

/* Two ATRAC3plus frames back to back, headers and all, and a third */
static uint8_t atrac[3][8 + 376];
static uint8_t audio[3 * (8 + 376)];

static void make_frames(void)
{
   unsigned f, i;
   for (f = 0; f < 3; f++)
   {
      atrac[f][0] = 0x0F; atrac[f][1] = 0xD0;
      atrac[f][2] = 0x28; atrac[f][3] = 0x2E;
      atrac[f][4] = atrac[f][5] = atrac[f][6] = atrac[f][7] = 0;
      for (i = 0; i < 376; i++)
         atrac[f][8 + i] = (uint8_t)(i * 7 + f * 31 + 1);
      memcpy(audio + f * (8 + 376), atrac[f], 8 + 376);
   }
}

static const uint8_t video[] = {
   0, 0, 0, 1, 0x09, 0xF0, 0, 0, 0, 1, 0x67, 0x4D, 0x40, 0x1E };

/* A movie: header, then video, then the three frames across three
 * packets of substream 0 with @skip header bytes each, the second
 * frame split mid-way, and, when @broken, junk where the third frame's
 * header should be continued */
static size_t build(uint8_t *file, unsigned skip, bool broken)
{
   uint8_t lead[8];
   size_t a = 8 + 376;
   unsigned i;
   ps_len = 0;
   memset(lead, 0xAA, sizeof(lead));
   lead[0] = 0x00;
   put_pack2(1000);
   put_pes2(0xE0, 9000, 6000, NULL, 0, video, sizeof(video));
   /* frame 0 whole, frame 1's first 100 bytes */
   put_pes2(0xBD, 9100, RPSMF_NO_PTS, lead, 1 + skip, audio, a + 100);
   put_pack2(2000);
   /* the rest of frame 1, and frame 2's header and 50 bytes */
   put_pes2(0xBD, 9500, RPSMF_NO_PTS, lead, 1 + skip, audio + a + 100,
         a - 100 + 8 + 50);
   if (broken)
      ps[ps_len - 50 - 8] ^= 0xFF; /* frame 2's header broken... */
   /* ...and the rest of it */
   put_pes2(0xBD, RPSMF_NO_PTS, RPSMF_NO_PTS, lead, 1 + skip,
         audio + 2 * a + 8 + 50, a - 8 - 50);
   /* a fresh frame after, which a broken stream recovers on */
   put_pes2(0xBD, 9900, RPSMF_NO_PTS, lead, 1 + skip, atrac[0], a);
   ps[ps_len++] = 0; ps[ps_len++] = 0; ps[ps_len++] = 1; ps[ps_len++] = 0xB9;

   memset(file, 0, 0x800);
   memcpy(file, "PSMF0015", 8);
   file[0x0A] = 0x08;                  /* data offset 0x800 */
   file[0x0C] = (uint8_t)(ps_len >> 24);
   file[0x0D] = (uint8_t)(ps_len >> 16);
   file[0x0E] = (uint8_t)(ps_len >> 8);
   file[0x0F] = (uint8_t)ps_len;
   file[0x57] = 0x01; file[0x58] = 0x5F; file[0x59] = 0x90; /* 90000 */
   file[0x5D] = 0x01;                     /* 65536 */
   file[0x62] = 0x12; file[0x63] = 0x34;
   for (i = 0; i < ps_len; i++)
      file[0x800 + i] = ps[i];
   return 0x800 + ps_len;
}

typedef struct
{
   unsigned video, frames, other;
   uint64_t video_pts, video_dts;
   uint64_t pts[8];
   bool     ok[8];
} seen_t;

static void drain(rpsmf_t *d, seen_t *s)
{
   rpsmf_packet_t p;
   while (rpsmf_next(d, &p))
   {
      if (p.kind == RPSMF_VIDEO)
      {
         CHECK(p.size == sizeof(video) && !memcmp(p.data, video, p.size),
               "video payload");
         s->video_pts = p.pts;
         s->video_dts = p.dts;
         s->video++;
      }
      else if (p.kind == RPSMF_ATRAC3P && s->frames < 8)
      {
         unsigned f = s->frames % 3;
         /* the last frame is atrac[0] again */
         if (s->frames >= 2)
            f = (p.pts == 9900) ? 0 : 2;
         s->ok[s->frames]  = p.size == 376 && p.params[0] == 0x28
            && p.params[1] == 0x2E && !memcmp(p.data, atrac[f] + 8, 376);
         s->pts[s->frames] = p.pts;
         s->frames++;
      }
      else
         s->other++;
   }
}

static void run(const uint8_t *file, size_t len, size_t chunk,
      seen_t *s)
{
   rpsmf_info_t info;
   rpsmf_t *d = rpsmf_init();
   size_t off;
   memset(s, 0, sizeof(*s));
   CHECK(rpsmf_parse_header(file, len, &info), "header");
   CHECK(info.version == 15 && info.data_offset == 0x800
         && info.tick_freq == 90000 && info.duration == 65536
         && info.mux_rate == 0x1234, "header fields");
   for (off = info.data_offset; off < len; )
   {
      size_t n = len - off < chunk ? len - off : chunk;
      size_t w = rpsmf_write(d, file + off, n);
      off += w;
      drain(d, s);
   }
   drain(d, s);
   rpsmf_free(d);
}

static uint8_t file[0x800 + (1 << 16)];

int main(void)
{
   static const size_t chunks[] = { (size_t)-1, 1, 7, 2048 };
   unsigned skip, c;
   make_frames();

   for (skip = 3; skip <= 5; skip += 2)
      for (c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++)
      {
         seen_t s;
         size_t len = build(file, skip, false);
         run(file, len, chunks[c], &s);
         CHECK(s.video == 1 && s.video_pts == 9000 && s.video_dts == 6000,
               "video PTS and DTS");
         CHECK(s.frames == 4, "four ATRAC3plus frames");
         CHECK(s.ok[0] && s.ok[1] && s.ok[2] && s.ok[3], "frames whole");
         /* a packet's PTS is the first frame to start in it */
         CHECK(s.pts[0] == 9100 && s.pts[1] == RPSMF_NO_PTS
               && s.pts[2] == 9500 && s.pts[3] == 9900, "frame PTS");
         CHECK(!s.other, "nothing else");

         len = build(file, skip, true);
         run(file, len, chunks[c], &s);
         /* the broken one goes; the stream comes back on the next */
         CHECK(s.frames == 3 && s.ok[0] && s.ok[1] && s.ok[2]
               && s.pts[2] == 9900, "recovers after a broken frame");
      }

   /* MPEG-1 still reads as it did */
   {
      static const uint8_t mpeg1[] = {
         0, 0, 1, 0xBA, 0x21, 0x00, 0x01, 0x00, 0x01, 0x80, 0x00, 0x01,
         0, 0, 1, 0xE0, 0, 9, 0x21, 0x00, 0x01, 0x00, 0x01, 0xAB, 0xCD,
         0xEF, 0x12,
         0, 0, 1, 0xB9 };
      rmpeg1_ps_packet_t p;
      rmpeg1_ps_t *m = rmpeg1_ps_init(0);
      rmpeg1_ps_write(m, mpeg1, sizeof(mpeg1));
      CHECK(rmpeg1_ps_next(m, &p) && p.type == RMPEG1_PS_VIDEO
            && p.size == 4 && p.pts == 0 && p.data[0] == 0xAB,
            "MPEG-1 PES");
      rmpeg1_ps_free(m);
   }

   /* Not a PSMF */
   {
      rpsmf_info_t info;
      memcpy(file, "PSMX0015", 8);
      CHECK(!rpsmf_parse_header(file, 0x800, &info), "bad magic");
      memcpy(file, "PSMF00x5", 8);
      CHECK(!rpsmf_parse_header(file, 0x800, &info), "bad version");
      CHECK(!rpsmf_parse_header(file, 10, &info), "short");
   }

   if (failures)
   {
      printf("rpsmf_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("rpsmf_test: all lanes pass\n");
   return 0;
}

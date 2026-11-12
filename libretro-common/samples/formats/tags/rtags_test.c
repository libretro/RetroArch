/* rtags against files built here byte by byte - each container, each
 * ID3v2 version and text encoding, unsynchronisation of a tag and of a
 * frame, Ogg comments across pages with another stream's page between,
 * the numbers FFmpeg turns into names and dates - then every prefix and
 * many corruptions of each, which must not read outside the file.
 *
 *   rtags_test                      the checks
 *   rtags_test --ffprobe FILE TAGS  FILE's tags against what ffprobe
 *                                   gave for it (-show_entries
 *                                   format_tags:stream_tags
 *                                   -of default=nw=1)
 * C89. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rtags.h>

static int failures;

#define CHECK(x) do { if (!(x)) { failures++; \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static const char *const names[RTAGS_FIELDS] = {
   "title", "artist", "album", "genre", "track", "disc", "date"
};

/* ---------------------------------------------------------------- */
/* building files                                                   */
/* ---------------------------------------------------------------- */

struct fx
{
   const char *name;
   uint8_t     d[4096];
   size_t      n;
};

static struct fx *cur;

static void put(const void *p, size_t n) { memcpy(cur->d + cur->n, p, n); cur->n += n; }
static void puts_(const char *s) { put(s, strlen(s)); }
static void put8(unsigned v) { cur->d[cur->n++] = (uint8_t)v; }
static void put16le(unsigned v) { put8(v & 0xff); put8((v >> 8) & 0xff); }
static void put16be(unsigned v) { put8((v >> 8) & 0xff); put8(v & 0xff); }
static void put32le(uint32_t v) { put16le(v & 0xffff); put16le(v >> 16); }
static void put32be(uint32_t v) { put16be(v >> 16); put16be(v & 0xffff); }
static void put64le(uint32_t v) { put32le(v); put32le(0); }
static void fill(unsigned v, size_t n) { while (n--) put8(v); }

static void set32be(size_t at, uint32_t v)
{
   cur->d[at] = (uint8_t)(v >> 24); cur->d[at + 1] = (uint8_t)(v >> 16);
   cur->d[at + 2] = (uint8_t)(v >> 8); cur->d[at + 3] = (uint8_t)v;
}
static void set32le(size_t at, uint32_t v)
{
   cur->d[at] = (uint8_t)v; cur->d[at + 1] = (uint8_t)(v >> 8);
   cur->d[at + 2] = (uint8_t)(v >> 16); cur->d[at + 3] = (uint8_t)(v >> 24);
}
static void set64le(size_t at, uint32_t v) { set32le(at, v); set32le(at + 4, 0); }
static void syncsafe(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)((v >> 21) & 0x7f); p[1] = (uint8_t)((v >> 14) & 0x7f);
   p[2] = (uint8_t)((v >> 7) & 0x7f);  p[3] = (uint8_t)(v & 0x7f);
}

/* UTF-8 (ASCII and the few characters used here) as UTF-16 */
static void put_utf16(const char *s, int be)
{
   const unsigned char *u = (const unsigned char*)s;
   while (*u)
   {
      unsigned long cp;
      if (*u < 0x80)                 { cp = *u; u += 1; }
      else if ((*u & 0xe0) == 0xc0)  { cp = ((unsigned long)(u[0] & 0x1f) << 6) | (u[1] & 0x3f); u += 2; }
      else if ((*u & 0xf0) == 0xe0)  { cp = ((unsigned long)(u[0] & 0x0f) << 12) | ((unsigned long)(u[1] & 0x3f) << 6) | (u[2] & 0x3f); u += 3; }
      else { cp = ((unsigned long)(u[0] & 0x07) << 18) | ((unsigned long)(u[1] & 0x3f) << 12) | ((unsigned long)(u[2] & 0x3f) << 6) | (u[3] & 0x3f); u += 4; }
      if (cp >= 0x10000)
      {
         cp -= 0x10000;
         if (be) { put16be(0xd800 + (unsigned)(cp >> 10)); put16be(0xdc00 + (unsigned)(cp & 0x3ff)); }
         else    { put16le(0xd800 + (unsigned)(cp >> 10)); put16le(0xdc00 + (unsigned)(cp & 0x3ff)); }
      }
      else if (be)
         put16be((unsigned)cp);
      else
         put16le((unsigned)cp);
   }
}

/* ID3v2.2/2.3 frames go into a scratch area, then into the tag -
 * unsynchronised, every 0xff followed by a 0x00, when @unsync */
static uint8_t frames[2048];
static size_t  frames_n;

static void frame(int v22, const char *id, const void *data, size_t len, unsigned flags)
{
   uint8_t *f = frames + frames_n;
   if (v22)
   {
      memcpy(f, id, 3);
      f[3] = (uint8_t)(len >> 16); f[4] = (uint8_t)(len >> 8); f[5] = (uint8_t)len;
      frames_n += 6;
   }
   else
   {
      memcpy(f, id, 4);
      f[4] = (uint8_t)(len >> 24); f[5] = (uint8_t)(len >> 16);
      f[6] = (uint8_t)(len >> 8);  f[7] = (uint8_t)len;
      f[8] = (uint8_t)(flags >> 8); f[9] = (uint8_t)flags;
      frames_n += 10;
   }
   memcpy(frames + frames_n, data, len);
   frames_n += len;
}

/* a text frame: the encoding byte, then @text as given */
static void tframe(int v22, const char *id, unsigned enc, const void *text, size_t len)
{
   uint8_t t[512];
   t[0] = (uint8_t)enc;
   memcpy(t + 1, text, len);
   frame(v22, id, t, len + 1, 0);
}

static void tframe_s(int v22, const char *id, const char *latin1)
{
   tframe(v22, id, 0, latin1, strlen(latin1));
}

static void id3_tag(unsigned ver, unsigned flags, int unsync, size_t padding)
{
   size_t at, i;
   puts_("ID3");
   put8(ver); put8(0); put8(flags);
   at = cur->n;
   put32be(0);
   for (i = 0; i < frames_n; i++)
   {
      put8(frames[i]);
      if (unsync && frames[i] == 0xff)
         put8(0);
   }
   fill(0, padding);
   syncsafe(cur->d + at, (uint32_t)(cur->n - at - 4));
   frames_n = 0;
}

/* an ID3v2.4 frame: syncsafe size, and its data as stored */
static void frame24(const char *id, const uint8_t *data, size_t len, unsigned flags)
{
   uint8_t *f = frames + frames_n;
   memcpy(f, id, 4);
   syncsafe(f + 4, (uint32_t)len);
   f[8] = (uint8_t)(flags >> 8); f[9] = (uint8_t)flags;
   frames_n += 10;
   memcpy(frames + frames_n, data, len);
   frames_n += len;
}

/* ---------------------------------------------------------------- */
/* the files                                                        */
/* ---------------------------------------------------------------- */

static struct fx fx_v23, fx_v24, fx_v22, fx_v1, fx_v2v1, fx_flac, fx_flac_id3,
                 fx_ea3, fx_ogg, fx_opus, fx_oggflac, fx_wav, fx_mp4, fx_qt, fx_asf;
static char      ogg_title[300];

static void build_v23(void)
{
   uint8_t t[64];
   size_t  n = 0;
   cur = &fx_v23; cur->name = "ID3v2.3";
   /* extended header: 6 bytes after its size; a CRC flag clear */
   frames[0] = 0; frames[1] = 0; frames[2] = 0; frames[3] = 6;
   memset(frames + 4, 0, 6);
   frames_n = 10;
   frame(0, "PRIV", "\xff\xe0owner\0\xff", 8, 0);
   /* UTF-16 with a BOM (0xff 0xfe, unsynchronised) */
   t[n++] = 1; t[n++] = 0xff; t[n++] = 0xfe;
   {
      struct fx tmp, *save = cur;
      cur = &tmp; tmp.n = 0;
      put_utf16("T\xc3\xaftle\xe2\x98\x83", 0);
      memcpy(t + n, tmp.d, tmp.n);
      n += tmp.n;
      cur = save;
   }
   t[n++] = 0; t[n++] = 0;
   frame(0, "TIT2", t, n, 0);
   tframe_s(0, "TPE1", "\xc4rtist");
   tframe_s(0, "TALB", "Alb\xff");
   /* a frame flagged compressed is passed over */
   frame(0, "TCON", "\0Classical", 10, 0x0080);
   tframe_s(0, "TCON", "(13)");
   tframe_s(0, "TRCK", "3/12");
   tframe_s(0, "TPOS", "1/2");
   tframe_s(0, "TYER", "2004");
   tframe_s(0, "TDAT", "0105");
   tframe_s(0, "TIME", "1230");
   /* a second title, which the first wins over */
   tframe_s(0, "TIT2", "Second");
   id3_tag(3, 0x80 | 0x40, 1, 16);
   fill(0xff, 4);   /* the audio, so to speak */
}

static void build_v24(void)
{
   uint8_t t[64];
   cur = &fx_v24; cur->name = "ID3v2.4";
   /* UTF-8, with a data length indicator */
   syncsafe(t, 10);
   memcpy(t + 4, "\x03T\xc3\xaftle\xe2\x98\x83", 10);
   frame24("TIT2", t, 14, 0x0001);
   /* unsynchronised: 0xff 0x00 0xff 0x00 stand for two 0xff */
   frame24("TPE1", (const uint8_t*)"\x00\xff\x00\xff\x00x", 6, 0x0002);
   /* UTF-16BE, a character outside the BMP */
   frame24("TALB", (const uint8_t*)"\x02\x00\x41\xd8\x3d\xde\x00", 7, 0);
   /* a compressed frame is passed over */
   frame24("TCON", (const uint8_t*)"\x00Opera", 6, 0x0008);
   /* two values; the first counts, and is a genre's number */
   frame24("TCON", (const uint8_t*)"\x03" "17\0Pop", 7, 0);
   /* UTF-16 with a big-endian BOM */
   frame24("TRCK", (const uint8_t*)"\x01\xfe\xff\x00\x35", 5, 0);
   /* grouped: a group byte before the encoding */
   frame24("TPOS", (const uint8_t*)"\x07\x00" "2/3", 5, 0x0040);
   /* a year of five digits does not count, as in FFmpeg: TDRC does */
   frame24("TYER", (const uint8_t*)"\x00" "20045", 6, 0);
   frame24("TDRC", (const uint8_t*)"\x00" "2004-05-01", 11, 0);
   id3_tag(4, 0, 0, 0);
   fill(0xff, 4);
}

static void build_v22(void)
{
   uint8_t t[32];
   cur = &fx_v22; cur->name = "ID3v2.2";
   tframe_s(1, "TT2", "Two");
   t[0] = 1; t[1] = 0xff; t[2] = 0xfe; t[3] = 'P'; t[4] = 0; t[5] = 'e'; t[6] = 0;
   frame(1, "TP1", t, 7, 0);
   tframe_s(1, "TCO", "Jazz Funk");
   tframe_s(1, "TYE", "1999");
   tframe_s(1, "TDA", "3112");
   tframe_s(1, "TRK", "4");
   id3_tag(2, 0, 0, 3);
   fill(0xff, 4);
}

/* OpenMG: an "ea3" tag, an ID3v2.3 tag by another name, then the
 * EA3 header and ATRAC frames */
static void build_ea3(void)
{
   cur = &fx_ea3; cur->name = "OpenMG";
   tframe(0, "TIT2", 2, "\x00O\x00m\x00" "a", 6);
   tframe_s(0, "TPE1", "Sony");
   id3_tag(3, 0, 0, 4);
   memcpy(cur->d, "ea3", 3);
   puts_("EA3\x01"); put16be(96); put16be(0xffff); fill(0, 88);
   fill(0x5a, 64);
}

static void id3v1(void)
{
   size_t at = cur->n;
   puts_("TAG");
   fill(0, 125);
   memcpy(cur->d + at + 3,  "Old Title   ", 12);
   memcpy(cur->d + at + 33, "Old Artist", 10);
   memcpy(cur->d + at + 63, "Old \xc5lbum", 10);   /* not UTF-8: ISO 8859-1 */
   memcpy(cur->d + at + 93, "1987", 4);
   memcpy(cur->d + at + 97, "a comment", 9);
   cur->d[at + 126] = 7;
   cur->d[at + 127] = 17;
}

static void build_v1(void)
{
   cur = &fx_v1; cur->name = "ID3v1";
   put8(0xff); put8(0xfb); fill(0x55, 198);
   id3v1();
}

static void build_v2v1(void)
{
   cur = &fx_v2v1; cur->name = "ID3v2 and ID3v1";
   tframe_s(0, "TIT2", "New Title");
   id3_tag(3, 0, 0, 0);
   put8(0xff); put8(0xfb); fill(0x55, 50);
   id3v1();
}

static void comment(const char *c)
{
   put32le((uint32_t)strlen(c));
   puts_(c);
}

static void vorbis_comment_block(int with_album)
{
   put32le(4); puts_("vend");
   put32le(with_album ? 10 : 9);
   comment("title=Flac Title");
   comment("ARTIST=A");
   comment("Artist=B");
   comment("ALBUMARTIST=Not The Album");
   comment("TRACKNUMBER=9");
   comment("DISCNUMBER=2");
   comment("DATE=2010");
   comment("GENRE=\xc3\x9cn\xc3\xaf");
   comment("NOEQUALS");
   if (with_album)
      comment("ALBUM=Real Album");
}

static void flac(int with_album)
{
   size_t at;
   puts_("fLaC");
   put8(0); put8(0); put8(0); put8(34); fill(0, 34);   /* STREAMINFO */
   put8(1); put8(0); put8(0); put8(4);  fill(0, 4);    /* PADDING */
   put8(0x84);
   at = cur->n;
   put8(0); put8(0); put8(0);
   vorbis_comment_block(with_album);
   {
      size_t len = cur->n - at - 3;
      cur->d[at] = (uint8_t)(len >> 16); cur->d[at + 1] = (uint8_t)(len >> 8);
      cur->d[at + 2] = (uint8_t)len;
   }
   fill(0xff, 8);
}

static void build_flac(void)
{
   cur = &fx_flac; cur->name = "FLAC";
   flac(1);
   cur = &fx_flac_id3; cur->name = "FLAC behind ID3v2";
   tframe_s(0, "TALB", "Id3 Album");
   tframe_s(0, "TCON", "Blues");
   id3_tag(3, 0, 0, 0);
   flac(0);
}

/* an Ogg page of @serial holding @len bytes in segments @lace */
static void ogg_page(uint32_t serial, unsigned flags, unsigned seq,
      const uint8_t *lace, unsigned nseg, const void *data, size_t len)
{
   puts_("OggS");
   put8(0); put8(flags);
   put32le(0); put32le(0);
   put32le(serial); put32le(seq); put32le(0);
   put8(nseg);
   put(lace, nseg);
   put(data, len);
}

static void build_ogg(void)
{
   static uint8_t pkt[1024];
   uint8_t        lace[8];
   size_t         n = 0, i;
   struct fx      tmp, *save;

   cur = &fx_ogg; cur->name = "Ogg Vorbis";
   /* identification: 30 bytes, alone on the first page */
   memcpy(pkt, "\x01vorbis", 7);
   memset(pkt + 7, 0, 23);
   lace[0] = 30;
   ogg_page(0x1234, 0x02, 0, lace, 1, pkt, 30);

   /* the comment header, its title across two pages */
   strcpy(ogg_title, "Ogg ");
   for (i = 4; i < 250; i++)
      ogg_title[i] = (char)('a' + i % 26);
   ogg_title[250] = '\0';
   save = cur; cur = &tmp; tmp.n = 0;
   puts_("\x03vorbis");
   put32le(3); puts_("lib");
   put32le(3);
   put32le((uint32_t)(6 + strlen(ogg_title))); puts_("TITLE="); puts_(ogg_title);
   comment("ARTIST=Ogg Artist");
   comment("TRACKNUMBER=1");
   put8(1);   /* framing */
   memcpy(pkt, tmp.d, tmp.n);
   n = tmp.n;
   cur = save;

   lace[0] = 255;
   ogg_page(0x1234, 0, 1, lace, 1, pkt, 255);
   /* another stream's page between */
   lace[0] = 9;
   ogg_page(0x9999, 0x02, 0, lace, 1, "TITLE=no!", 9);
   /* the rest, then the setup header's start */
   lace[0] = (uint8_t)(n - 255); lace[1] = 4;
   {
      uint8_t rest[300];
      memcpy(rest, pkt + 255, n - 255);
      memcpy(rest + n - 255, "\x05vor", 4);
      ogg_page(0x1234, 0x01, 2, lace, 2, rest, n - 255 + 4);
   }

   cur = &fx_opus; cur->name = "Ogg Opus";
   memcpy(pkt, "OpusHead", 8);
   memset(pkt + 8, 1, 11);
   lace[0] = 19;
   ogg_page(7, 0x02, 0, lace, 1, pkt, 19);
   save = cur; cur = &tmp; tmp.n = 0;
   puts_("OpusTags");
   put32le(0);
   put32le(2);
   comment("artist=Opus Artist");
   comment("date=2020-01-01");
   memcpy(pkt, tmp.d, tmp.n);
   n = tmp.n;
   cur = save;
   lace[0] = (uint8_t)n;
   ogg_page(7, 0, 1, lace, 1, pkt, n);

   cur = &fx_oggflac; cur->name = "Ogg FLAC";
   save = cur; cur = &tmp; tmp.n = 0;
   puts_("\x7f" "FLAC"); put8(1); put8(0); put16be(1);
   puts_("fLaC"); put8(0); put8(0); put8(0); put8(34); fill(0, 34);
   memcpy(pkt, tmp.d, tmp.n);
   n = tmp.n;
   cur = save;
   lace[0] = (uint8_t)n;
   ogg_page(5, 0x02, 0, lace, 1, pkt, n);
   save = cur; cur = &tmp; tmp.n = 0;
   put8(0x84); put8(0); put8(0); put8(0);
   put32le(0); put32le(1);
   comment("ALBUM=Ogg Flac Album");
   memcpy(pkt, tmp.d, tmp.n);
   n = tmp.n;
   cur = save;
   lace[0] = (uint8_t)n;
   ogg_page(5, 0, 1, lace, 1, pkt, n);
}

static void build_wav(void)
{
   size_t riff, list;
   cur = &fx_wav; cur->name = "WAVE";
   puts_("RIFF"); riff = cur->n; put32le(0); puts_("WAVE");
   puts_("fmt "); put32le(16); fill(1, 16);
   puts_("LIST"); list = cur->n; put32le(0); puts_("INFO");
   puts_("INAM"); put32le(5); put("Wave\0", 5); put8(0);
   puts_("IART"); put32le(7); put("\xc9mile\0\0", 7); put8(0);
   puts_("ITRK"); put32le(3); put("11\0", 3); put8(0);
   set32le(list, (uint32_t)(cur->n - list - 4));
   puts_("data"); put32le(4); fill(0, 4);
   {
      size_t at;
      puts_("id3 "); at = cur->n; put32le(0);
      tframe_s(0, "TALB", "Chunk Album");
      tframe_s(0, "TIT2", "Not This");
      id3_tag(3, 0, 0, 0);
      set32le(at, (uint32_t)(cur->n - at - 4));
   }
   set32le(riff, (uint32_t)(cur->n - 8));
}

/* MP4: a box, its size patched when it closes */
static size_t box_open(const char *type) { size_t at = cur->n; put32be(0); puts_(type); return at; }
static void box_close(size_t at) { set32be(at, (uint32_t)(cur->n - at)); }

static void item(const char *key, unsigned type, const void *v, size_t len)
{
   size_t i = box_open(key), d = box_open("data");
   put32be(type); put32be(0);
   put(v, len);
   box_close(d);
   box_close(i);
}

static void build_mp4(void)
{
   size_t moov, udta, meta, ilst, b;
   cur = &fx_mp4; cur->name = "MP4";
   b = box_open("ftyp"); puts_("M4A "); put32be(0); box_close(b);
   moov = box_open("moov");
   b = box_open("mvhd"); fill(0, 8); box_close(b);
   udta = box_open("udta");
   meta = box_open("meta"); put32be(0);
   b = box_open("hdlr"); fill(0, 8); puts_("mdirappl"); fill(0, 9); box_close(b);
   ilst = box_open("ilst");
   item("\xa9nam", 1, "Mp4 T\xc3\xaftle", 11);
   item("\xa9" "ART", 1, "Mp4 Artist", 10);
   item("trkn", 0, "\0\0\0\x03\0\x0c\0\0", 8);
   item("disk", 0, "\0\0\0\x01\0\0", 6);
   item("gnre", 0, "\0\x12", 2);
   item("\xa9" "day", 1, "2001-02-03", 10);
   box_close(ilst);
   box_close(meta);
   box_close(udta);
   box_close(moov);
   b = box_open("mdat"); fill(0, 16); box_close(b);

   cur = &fx_qt; cur->name = "QuickTime";
   b = box_open("ftyp"); puts_("qt  "); put32be(0); box_close(b);
   b = box_open("mdat"); fill(0, 16); box_close(b);
   moov = box_open("moov");
   udta = box_open("udta");
   meta = box_open("meta");   /* QuickTime's: no version and flags */
   b = box_open("hdlr"); fill(0, 8); puts_("mdirappl"); fill(0, 9); box_close(b);
   ilst = box_open("ilst");
   item("\xa9" "ART", 1, "QT Artist", 9);
   item("trkn", 0, "\0\0\0\x05", 4);
   box_close(ilst);
   box_close(meta);
   /* the file ends with the last string's last byte */
   b = box_open("\xa9nam"); put16be(8); put16be(0x55c4); puts_("QT Title"); box_close(b);
   b = box_open("\xa9" "alb"); put16be(8); put16be(0x55c4); puts_("QT Album"); box_close(b);
   box_close(udta);
   box_close(moov);
}

static const uint8_t g_header[16]  = { 0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0x00,0xaa,0x00,0x62,0xce,0x6c };
static const uint8_t g_content[16] = { 0x33,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0x00,0xaa,0x00,0x62,0xce,0x6c };
static const uint8_t g_ext[16]     = { 0x40,0xa4,0xd0,0xd2,0x07,0xe3,0xd2,0x11,0x97,0xf0,0x00,0xa0,0xc9,0x5e,0xa8,0x50 };
static const uint8_t g_other[16]   = { 0xa1,0xdc,0xab,0x8c,0x47,0xa9,0xcf,0x11,0x8e,0xe4,0x00,0xc0,0x0c,0x20,0x53,0x65 };

static void asf_name(const char *n)
{
   put16le((unsigned)(strlen(n) + 1) * 2);
   put_utf16(n, 0);
   put16le(0);
}

static void build_asf(void)
{
   size_t hdr, obj;
   cur = &fx_asf; cur->name = "ASF";
   put(g_header, 16); hdr = cur->n; put64le(0); put32le(3); put8(1); put8(2);

   /* an object of no interest */
   put(g_other, 16); obj = cur->n; put64le(0); fill(0, 20);
   set64le(obj, (uint32_t)(cur->n - obj + 16));

   put(g_content, 16); obj = cur->n; put64le(0);
   put16le(20); put16le(16); put16le(0); put16le(0); put16le(0);
   put_utf16("Wma T\xc3\xaftle", 0); put16le(0);
   put_utf16("Wma Art", 0); put16le(0);
   set64le(obj, (uint32_t)(cur->n - obj + 16));

   put(g_ext, 16); obj = cur->n; put64le(0);
   put16le(5);
   asf_name("WM/AlbumTitleX"); put16le(0); put16le(6); put_utf16("No", 0); put16le(0);
   asf_name("WM/AlbumTitle");  put16le(0); put16le(20); put_utf16("Wma Album", 0); put16le(0);
   asf_name("WM/Genre");       put16le(1); put16le(2); put16le(0x4142);   /* bytes: none */
   asf_name("WM/TrackNumber"); put16le(3); put16le(4); put32le(5);
   /* unterminated, and the last bytes of the file */
   asf_name("WM/Year");        put16le(0); put16le(8); put_utf16("1995", 0);
   set64le(obj, (uint32_t)(cur->n - obj + 16));

   set64le(hdr, (uint32_t)(cur->n - hdr + 16));
}

/* ---------------------------------------------------------------- */

static void expect(const struct fx *f, enum rtags_field field, const char *want)
{
   char   out[512];
   size_t n = rtags_get(f->d, f->n, field, out, sizeof(out));
   if (n != strlen(want) || strcmp(out, want) || strlen(out) != n)
   {
      failures++;
      fprintf(stderr, "%s: %s is \"%s\", not \"%s\"\n", f->name, names[field], out, want);
   }
}

static void expect_all(const struct fx *f, const char *const want[RTAGS_FIELDS])
{
   int k;
   for (k = 0; k < RTAGS_FIELDS; k++)
      expect(f, (enum rtags_field)k, want[k]);
}

/* every prefix of a file, and each byte of it set to 0x00, 0xff and
 * flipped: whatever comes out is a string of the length returned. Each
 * copy is allocated to its size, for ASan to see a read past its end */
static void harden(const struct fx *f)
{
   char   out[40];
   size_t len, i;
   int    k, m;
   for (len = 0; len <= f->n; len++)
   {
      uint8_t *copy = (uint8_t*)malloc(len ? len : 1);
      memcpy(copy, f->d, len);
      for (k = 0; k < RTAGS_FIELDS; k++)
         CHECK(rtags_get(copy, len, (enum rtags_field)k, out, sizeof(out)) == strlen(out));
      free(copy);
   }
   for (i = 0; i < f->n; i++)
      for (m = 0; m < 3; m++)
      {
         uint8_t *copy = (uint8_t*)malloc(f->n);
         memcpy(copy, f->d, f->n);
         copy[i] = (uint8_t)(m == 0 ? 0 : m == 1 ? 0xff : ~copy[i]);
         for (k = 0; k < RTAGS_FIELDS; k++)
            CHECK(rtags_get(copy, f->n, (enum rtags_field)k, out, sizeof(out)) == strlen(out));
         free(copy);
      }
}

static int run_checks(void)
{
   static const char *const v23[]  = { "T\xc3\xaftle\xe2\x98\x83", "\xc3\x84rtist", "Alb\xc3\xbf", "Pop", "3/12", "1/2", "2004-05-01 12:30" };
   static const char *const v24[]  = { "T\xc3\xaftle\xe2\x98\x83", "\xc3\xbf\xc3\xbfx", "A\xf0\x9f\x98\x80", "Rock", "5", "2/3", "2004-05-01" };
   static const char *const v22[]  = { "Two", "Pe", "", "Jazz Funk", "4", "", "1999-12-31" };
   static const char *const v1[]   = { "Old Title", "Old Artist", "Old \xc3\x85lbum", "Rock", "7", "", "1987" };
   static const char *const v2v1[] = { "New Title", "Old Artist", "Old \xc3\x85lbum", "Rock", "7", "", "1987" };
   static const char *const fl[]   = { "Flac Title", "A;B", "Real Album", "\xc3\x9cn\xc3\xaf", "9", "2", "2010" };
   static const char *const fl3[]  = { "Flac Title", "A;B", "Id3 Album", "\xc3\x9cn\xc3\xaf", "9", "2", "2010" };
   static const char *const opus[] = { "", "Opus Artist", "", "", "", "", "2020-01-01" };
   static const char *const ofl[]  = { "", "", "Ogg Flac Album", "", "", "", "" };
   static const char *const wav[]  = { "Wave", "\xc3\x89mile", "Chunk Album", "", "11", "", "" };
   static const char *const mp4[]  = { "Mp4 T\xc3\xaftle", "Mp4 Artist", "", "Rock", "3/12", "1", "2001-02-03" };
   static const char *const qt[]   = { "QT Title", "QT Artist", "QT Album", "", "5", "", "" };
   static const char *const ea3[]  = { "Oma", "Sony", "", "", "", "", "" };
   static const char *const asf[]  = { "Wma T\xc3\xaftle", "Wma Art", "Wma Album", "", "5", "", "1995" };
   const char *ogg[RTAGS_FIELDS];
   struct fx  *all[15];
   char        small[4];
   size_t      i;

   build_v23(); build_v24(); build_v22(); build_v1(); build_v2v1();
   build_flac(); build_ea3(); build_ogg(); build_wav(); build_mp4(); build_asf();

   expect_all(&fx_v23, v23);
   expect_all(&fx_v24, v24);
   expect_all(&fx_v22, v22);
   expect_all(&fx_v1, v1);
   expect_all(&fx_v2v1, v2v1);
   expect_all(&fx_flac, fl);
   expect_all(&fx_flac_id3, fl3);
   for (i = 0; i < RTAGS_FIELDS; i++)
      ogg[i] = "";
   ogg[RTAGS_TITLE]  = ogg_title;
   ogg[RTAGS_ARTIST] = "Ogg Artist";
   ogg[RTAGS_TRACK]  = "1";
   expect_all(&fx_ogg, ogg);
   expect_all(&fx_opus, opus);
   expect_all(&fx_oggflac, ofl);
   expect_all(&fx_wav, wav);
   expect_all(&fx_mp4, mp4);
   expect_all(&fx_qt, qt);
   expect_all(&fx_asf, asf);
   expect_all(&fx_ea3, ea3);

   /* a value cut where it no longer fits, never inside a character */
   CHECK(rtags_get(fx_v24.d, fx_v24.n, RTAGS_TITLE, small, 4) == 3 && !strcmp(small, "T\xc3\xaf"));
   CHECK(rtags_get(fx_v24.d, fx_v24.n, RTAGS_TITLE, small, 3) == 1 && !strcmp(small, "T"));
   CHECK(rtags_get(fx_v24.d, fx_v24.n, RTAGS_TITLE, small, 1) == 0 && !small[0]);
   CHECK(rtags_get(fx_v24.d, fx_v24.n, RTAGS_TITLE, NULL, 0) == 0);
   CHECK(rtags_get(NULL, 0, RTAGS_TITLE, small, 4) == 0 && !small[0]);
   CHECK(rtags_get(fx_v24.d, fx_v24.n, RTAGS_FIELDS, small, 4) == 0 && !small[0]);

   CHECK(!strcmp(rtags_genre_name(0), "Blues"));
   CHECK(!strcmp(rtags_genre_name(191), "Psybient"));
   CHECK(rtags_genre_name(192) == NULL);

   all[0] = &fx_v23; all[1] = &fx_v24; all[2] = &fx_v22; all[3] = &fx_v1;
   all[4] = &fx_v2v1; all[5] = &fx_flac; all[6] = &fx_flac_id3; all[7] = &fx_ogg;
   all[8] = &fx_opus; all[9] = &fx_oggflac; all[10] = &fx_wav; all[11] = &fx_mp4;
   all[12] = &fx_qt; all[13] = &fx_asf; all[14] = &fx_ea3;
   for (i = 0; i < 15; i++)
      harden(all[i]);

   if (failures)
   {
      fprintf(stderr, "rtags_test: %d check(s) failed\n", failures);
      return 1;
   }
   printf("rtags_test: ok (15 files, every prefix and byte corrupted)\n");
   return 0;
}

/* ---------------------------------------------------------------- */

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE    *f = fopen(path, "rb");
   uint8_t *p;
   long     n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   p = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
   if (p && fread(p, 1, (size_t)n, f) != (size_t)n)
   {
      free(p);
      p = NULL;
   }
   fclose(f);
   *len = (size_t)n;
   return p;
}

static int key_is(const char *a, const char *b, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      char x = a[i] >= 'A' && a[i] <= 'Z' ? (char)(a[i] + 32) : a[i];
      if (x != b[i])
         return 0;
   }
   return b[n] == '\0';
}

/* FILE's tags against ffprobe's "TAG:key=value" lines for it - its
 * first value under each key, format tags before stream tags */
static int run_ffprobe(const char *file, const char *tags)
{
   size_t   len = 0, tlen = 0;
   uint8_t *data = slurp(file, &len);
   uint8_t *t    = slurp(tags, &tlen);
   int      k, bad = 0;
   if (!data || !t)
   {
      fprintf(stderr, "rtags_test: cannot read %s or %s\n", file, tags);
      return 1;
   }
   for (k = 0; k < RTAGS_FIELDS; k++)
   {
      char        out[512], want[512];
      const char *p = (const char*)t, *end = (const char*)t + tlen;
      want[0] = '\0';
      while (p < end)
      {
         const char *nl = (const char*)memchr(p, '\n', (size_t)(end - p));
         const char *eq;
         if (!nl)
            nl = end;
         eq = (const char*)memchr(p, '=', (size_t)(nl - p));
         if (eq && nl - p > 4 && !memcmp(p, "TAG:", 4) && key_is(p + 4, names[k], (size_t)(eq - p - 4)))
         {
            size_t n = (size_t)(nl - eq - 1);
            if (n >= sizeof(want))
               n = sizeof(want) - 1;
            memcpy(want, eq + 1, n);
            want[n] = '\0';
            break;
         }
         p = nl + 1;
      }
      rtags_get(data, len, (enum rtags_field)k, out, sizeof(out));
      if (strcmp(out, want))
      {
         fprintf(stderr, "%s: %s is \"%s\", ffprobe's \"%s\"\n", file, names[k], out, want);
         bad = 1;
      }
   }
   free(data);
   free(t);
   if (!bad)
      printf("rtags_test: %s: as ffprobe reads it\n", file);
   return bad;
}

int main(int argc, char **argv)
{
   if (argc == 4 && !strcmp(argv[1], "--ffprobe"))
      return run_ffprobe(argv[2], argv[3]);
   return run_checks();
}

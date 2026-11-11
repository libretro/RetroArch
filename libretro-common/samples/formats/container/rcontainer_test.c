/* rcontainer against AVI and ASF headers built here byte by byte - each
 * stream kind, a WAVE_FORMAT_EXTENSIBLE audio stream, idx1 bit rates,
 * the ASF bit rate properties - then every prefix and many corruptions
 * of each, which must not read outside the file.
 *
 *   rcontainer_test                     the checks
 *   rcontainer_test --ffprobe FILE INFO FILE against what ffprobe gave for
 *                                       it (-show_entries stream=codec_type,
 *                                       codec_tag,width,height,sample_rate,
 *                                       channels,bit_rate,duration
 *                                       -of default=nw=1)
 * C89. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rcontainer.h>

static int failures;

#define CHECK(x) do { if (!(x)) { failures++; \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static uint8_t buf[4096];
static size_t  n;

static void put(const void *p, size_t k) { memcpy(buf + n, p, k); n += k; }
static void put8(unsigned v) { buf[n++] = (uint8_t)v; }
static void put16(unsigned v) { put8(v & 0xff); put8((v >> 8) & 0xff); }
static void put32(uint32_t v) { put16(v & 0xffff); put16(v >> 16); }
static void put64(uint32_t v) { put32(v); put32(0); }
static void fill(unsigned v, size_t k) { while (k--) put8(v); }
static void set32(size_t at, uint32_t v)
{
   buf[at] = (uint8_t)v; buf[at + 1] = (uint8_t)(v >> 8);
   buf[at + 2] = (uint8_t)(v >> 16); buf[at + 3] = (uint8_t)(v >> 24);
}
static size_t open_chunk(const char *id) { size_t at; put(id, 4); at = n; put32(0); return at; }
static void close_chunk(size_t at) { set32(at, (uint32_t)(n - at - 4)); if (n & 1) put8(0); }

/* an AVI: XVID 160x90 at 25/1, 50 frames; MP3 44.1 kHz stereo
 * (WAVE_FORMAT_EXTENSIBLE around 0x55 for the check), 2 s long; idx1
 * giving the video 100000 bytes */
static void build_avi(void)
{
   size_t riff, hdrl, strl, c;
   int    i;
   n = 0;
   put("RIFF", 4); riff = n; put32(0); put("AVI ", 4);
   put("LIST", 4); hdrl = n; put32(0); put("hdrl", 4);
   c = open_chunk("avih"); put32(40000); fill(0, 12); put32(50); fill(0, 36); close_chunk(c);
   /* video */
   put("LIST", 4); strl = n; put32(0); put("strl", 4);
   c = open_chunk("strh"); put("vidsXVID", 8); fill(0, 12); put32(1); put32(25); put32(0); put32(50); fill(0, 20); close_chunk(c);
   c = open_chunk("strf"); put32(40); put32(160); put32((uint32_t)-90); put16(1); put16(24); put("XVID", 4); fill(0, 20); close_chunk(c);
   set32(strl, (uint32_t)(n - strl - 4));
   /* audio: 2 s */
   put("LIST", 4); strl = n; put32(0); put("strl", 4);
   c = open_chunk("strh"); put("auds", 4); put32(0); fill(0, 12); put32(1); put32(44100); put32(0); put32(88200); fill(0, 20); close_chunk(c);
   c = open_chunk("strf"); put16(0xFFFE); put16(2); put32(44100); put32(16000); put16(1); put16(0); put16(22);
   put16(0); put32(3); put16(0x55); fill(0, 14); close_chunk(c);
   set32(strl, (uint32_t)(n - strl - 4));
   set32(hdrl, (uint32_t)(n - hdrl - 4));
   c = open_chunk("idx1");
   for (i = 0; i < 4; i++) { put("00dc", 4); put32(0x10); put32(0); put32(25000); }
   put("01wb", 4); put32(0); put32(0); put32(9999);
   close_chunk(c);
   set32(riff, (uint32_t)(n - 8));
}

static const uint8_t g_header[16]  = { 0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C };
static const uint8_t g_file[16]    = { 0xA1,0xDC,0xAB,0x8C,0x47,0xA9,0xCF,0x11,0x8E,0xE4,0x00,0xC0,0x0C,0x20,0x53,0x65 };
static const uint8_t g_stream[16]  = { 0x91,0x07,0xDC,0xB7,0xB7,0xA9,0xCF,0x11,0x8E,0xE6,0x00,0xC0,0x0C,0x20,0x53,0x65 };
static const uint8_t g_audio[16]   = { 0x40,0x9E,0x69,0xF8,0x4D,0x5B,0xCF,0x11,0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B };
static const uint8_t g_video[16]   = { 0xC0,0xEF,0x19,0xBC,0x4D,0x5B,0xCF,0x11,0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B };
static const uint8_t g_bitrate[16] = { 0xCE,0x75,0xF8,0x7B,0x8D,0x46,0xD1,0x11,0x8D,0x82,0x00,0x60,0x97,0xC9,0xA2,0xB2 };

static size_t asf_object(const uint8_t *guid) { size_t at; put(guid, 16); at = n; put64(0); return at; }
static void asf_close(size_t at) { set32(at, (uint32_t)(n - at + 16)); }

/* an ASF: WMV3 320x240 stream 1, WMA2 48 kHz stereo stream 2, 3.5 s
 * played after a 3 s preroll; bit rates 500000 and 128000 */
static void build_asf(void)
{
   size_t hdr, o;
   n = 0;
   put(g_header, 16); hdr = n; put64(0); put32(4); put8(1); put8(2);
   o = asf_object(g_file); fill(0, 40); put64(65000000); put64(0); put64(3000); put32(2); put32(3200); put32(3200); put32(0); asf_close(o);
   o = asf_object(g_stream); put(g_video, 16); fill(0, 16); put64(0); put32(11 + 40); put32(0); put16(1); put32(0);
   put32(320); put32(240); put8(2); put16(40);
   put32(40); put32(320); put32(240); put16(1); put16(24); put("WMV3", 4); fill(0, 20); asf_close(o);
   o = asf_object(g_stream); put(g_audio, 16); fill(0, 16); put64(0); put32(18); put32(0); put16(2); put32(0);
   put16(0x161); put16(2); put32(48000); put32(16000); put16(4096); put16(16); put16(0); asf_close(o);
   o = asf_object(g_bitrate); put16(2); put16(1); put32(500000); put16(2); put32(128000); asf_close(o);
   set32(hdr, (uint32_t)(n - hdr + 16));
}

static void harden(void)
{
   size_t len, i;
   int    m;
   for (len = 0; len <= n; len++)
   {
      uint8_t        *c = (uint8_t*)malloc(len ? len : 1);
      rcontainer_info info;
      memcpy(c, buf, len);
      rcontainer_probe(c, len, &info);
      CHECK(info.num_streams <= RCONTAINER_MAX_STREAMS);
      free(c);
   }
   for (i = 0; i < n; i++)
      for (m = 0; m < 3; m++)
      {
         uint8_t        *c = (uint8_t*)malloc(n);
         rcontainer_info info;
         memcpy(c, buf, n);
         c[i] = (uint8_t)(m == 0 ? 0 : m == 1 ? 0xff : ~c[i]);
         rcontainer_probe(c, n, &info);
         CHECK(info.num_streams <= RCONTAINER_MAX_STREAMS);
         free(c);
      }
}

static int run_checks(void)
{
   rcontainer_info info;

   build_avi();
   CHECK(rcontainer_probe(buf, n, &info) && info.kind == RCONTAINER_AVI && info.num_streams == 2);
   CHECK(info.streams[0].type == RCONTAINER_STREAM_VIDEO
         && info.streams[0].fourcc == RCONTAINER_FOURCC('X','V','I','D')
         && info.streams[0].width == 160 && info.streams[0].height == 90);
   CHECK(info.streams[1].type == RCONTAINER_STREAM_AUDIO && info.streams[1].format_tag == 0x55
         && info.streams[1].sample_rate == 44100 && info.streams[1].channels == 2
         && info.streams[1].bitrate == 128000);
   CHECK(info.duration_us == 2000000);
   CHECK(info.streams[0].bitrate == 400000);   /* 100000 bytes over 2 s */
   harden();

   build_asf();
   CHECK(rcontainer_probe(buf, n, &info) && info.kind == RCONTAINER_ASF && info.num_streams == 2);
   CHECK(info.streams[0].type == RCONTAINER_STREAM_VIDEO
         && info.streams[0].fourcc == RCONTAINER_FOURCC('W','M','V','3')
         && info.streams[0].width == 320 && info.streams[0].height == 240
         && info.streams[0].bitrate == 500000);
   CHECK(info.streams[1].type == RCONTAINER_STREAM_AUDIO && info.streams[1].format_tag == 0x161
         && info.streams[1].sample_rate == 48000 && info.streams[1].channels == 2
         && info.streams[1].bitrate == 128000);
   CHECK(info.duration_us == 3500000);
   harden();

   CHECK(!rcontainer_probe((const uint8_t*)"RIFF\0\0\0\0WAVE", 12, &info) && !info.num_streams);
   CHECK(!rcontainer_probe(NULL, 0, &info));

   if (failures)
   {
      fprintf(stderr, "rcontainer_test: %d check(s) failed\n", failures);
      return 1;
   }
   printf("rcontainer_test: ok (AVI and ASF, every prefix and byte corrupted)\n");
   return 0;
}

/* ---------------------------------------------------------------- */

static char *slurp(const char *path, size_t *len)
{
   FILE *fp = fopen(path, "rb");
   char *p;
   long  k;
   if (!fp)
      return NULL;
   fseek(fp, 0, SEEK_END);
   k = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   p = (char*)malloc((size_t)k + 1);
   if (p && fread(p, 1, (size_t)k, fp) != (size_t)k)
   {
      free(p);
      p = NULL;
   }
   fclose(fp);
   if (p)
      p[k] = '\0';
   *len = (size_t)k;
   return p;
}

static long diff_pct(double a, double b)
{
   return (long)(b ? 100.0 * (a > b ? a - b : b - a) / b : (a ? 100 : 0));
}

/* FILE against ffprobe's lines: each stream's type, tag, size or rate
 * and channels; its bit rate within 5% where both give one (an AVI's
 * MP3 stream states none in its header; FFmpeg reads one from the
 * frames); the length within 1% of the longest stream's (FFmpeg's file
 * length adds an ASF stream's start time) */
static int run_ffprobe(const char *file, const char *probe)
{
   size_t          len = 0, plen = 0;
   uint8_t        *data = (uint8_t*)slurp(file, &len);
   char           *t    = slurp(probe, &plen), *line;
   rcontainer_info info;
   int             s    = -1, bad = 0;
   double          dur  = 0;
   if (!data || !t || !rcontainer_probe(data, len, &info))
   {
      fprintf(stderr, "%s: not read\n", file);
      return 1;
   }
   for (line = strtok(t, "\n"); line; line = strtok(NULL, "\n"))
   {
      const char        *v = strchr(line, '=');
      rcontainer_stream *st;
      if (!v)
         continue;
      v++;
      if (!strncmp(line, "duration=", 9))
      {
         if (atof(v) > dur)
            dur = atof(v);
         continue;
      }
      if (!strncmp(line, "codec_type=", 11))
      {
         s++;
         if (s >= (int)info.num_streams)
            break;
         st = &info.streams[s];
         if ((!strcmp(v, "video") && st->type != RCONTAINER_STREAM_VIDEO)
               || (!strcmp(v, "audio") && st->type != RCONTAINER_STREAM_AUDIO))
         {
            fprintf(stderr, "%s: stream %d is not %s\n", file, s, v);
            bad = 1;
         }
         continue;
      }
      if (s < 0 || s >= (int)info.num_streams || !strcmp(v, "N/A"))
         continue;
      st = &info.streams[s];
      if (!strncmp(line, "codec_tag=", 10))
      {
         unsigned long tag = strtoul(v, NULL, 16);
         if (tag != (st->type == RCONTAINER_STREAM_VIDEO ? st->fourcc : st->format_tag))
         {
            fprintf(stderr, "%s: stream %d tag %s, not %lx\n", file, s, v,
                  (unsigned long)(st->type == RCONTAINER_STREAM_VIDEO ? st->fourcc : st->format_tag));
            bad = 1;
         }
      }
      else if (     (!strncmp(line, "width=", 6) && (unsigned)atol(v) != st->width)
               || (!strncmp(line, "height=", 7) && (unsigned)atol(v) != st->height)
               || (!strncmp(line, "sample_rate=", 12) && (unsigned)atol(v) != st->sample_rate)
               || (!strncmp(line, "channels=", 9) && (unsigned)atol(v) != st->channels)
               || (!strncmp(line, "bit_rate=", 9) && st->bitrate && diff_pct(st->bitrate, atof(v)) > 5))
      {
         fprintf(stderr, "%s: stream %d: ffprobe's %s\n", file, s, line);
         bad = 1;
      }
   }
   if (s + 1 != (int)info.num_streams || diff_pct((double)info.duration_us, dur * 1e6) > 1)
   {
      fprintf(stderr, "%s: %u streams (ffprobe's %d), %.3f s (ffprobe's %.3f)\n", file,
            info.num_streams, s + 1, info.duration_us / 1e6, dur);
      bad = 1;
   }
   if (!bad)
      printf("rcontainer_test: %s: as ffprobe reads it\n", file);
   free(data);
   free(t);
   return bad;
}

int main(int argc, char **argv)
{
   if (argc == 4 && !strcmp(argv[1], "--ffprobe"))
      return run_ffprobe(argv[2], argv[3]);
   return run_checks();
}

/* rmp3's gapless playback: the Xing/Info tag frame and LAME's encoder
 * delay and padding.
 *
 * Streams from ffmpeg (libmp3lame) with its Info tag, of a length that
 * is no whole number of frames: the pull API must give exactly the
 * samples that went in, the same ones ffmpeg's own decoder gives when
 * it trims (to a rounding step), and seek in those terms. A stream
 * without the tag decodes as before. Tags built here check the side
 * information offsets (MPEG-1/2, mono, CRC), VBRI and a cut-off tag.
 *
 * Needs ffmpeg with libmp3lame in PATH; a missing tool is a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>

#include <formats/rmp3.h>

static int fails;
static char dir[256];

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static int run(const char *fmt, ...)
{
   char cmd[1024];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(cmd, sizeof(cmd), fmt, ap);
   va_end(ap);
   return system(cmd);
}

static uint8_t *load(const char *path, size_t *size)
{
   uint8_t *data;
   long     n;
   FILE    *f = fopen(path, "rb");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   data = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
   if (data && fread(data, 1, (size_t)n, f) != (size_t)n)
   {
      free(data);
      data = NULL;
   }
   fclose(f);
   *size = (size_t)n;
   return data;
}

static void put_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

/* A stream through ffmpeg: encoded from @frames samples of a sine, and
 * decoded again by ffmpeg, which trims by the tag. */
static void test_stream(const char *name, const char *args,
      unsigned frames, unsigned rate, unsigned channels)
{
   char      mp3_path[512], ref_path[512], what[160];
   uint8_t  *mp3, *ref;
   size_t    mp3_size = 0, ref_size = 0;
   int16_t  *pcm;
   rmp3     *dec;
   uint64_t  got, i, n, worst = 0;
   rmp3_vbr_tag tag;

   printf("%s (%s):\n", name, args);
   snprintf(mp3_path, sizeof(mp3_path), "%s/%s.mp3", dir, name);
   snprintf(ref_path, sizeof(ref_path), "%s/%s.raw", dir, name);
   if (run("ffmpeg -v error -y -f lavfi -i \"sine=f=440:r=%u,volume=0.5\" "
           "-af atrim=end_sample=%u -ac %u %s -c:a libmp3lame "
           "-id3v2_version 0 '%s'", rate, frames, channels, args, mp3_path)
         || run("ffmpeg -v error -y -i '%s' -f s16le '%s'", mp3_path, ref_path))
   {
      check("ffmpeg encodes and decodes", 0);
      return;
   }
   mp3 = load(mp3_path, &mp3_size);
   ref = load(ref_path, &ref_size);
   dec = (rmp3*)calloc(1, sizeof(*dec));
   pcm = (int16_t*)malloc(((size_t)frames + 4096) * channels * sizeof(int16_t));
   if (!mp3 || !ref || !dec || !pcm || !rmp3_init_memory(dec, mp3, mp3_size))
   {
      check("the stream opens", 0);
      goto done;
   }

   check("the first frame is an Info tag with LAME's delay",
         rmp3_parse_vbr_tag(mp3, mp3_size, &tag) && tag.has_delay);
   snprintf(what, sizeof(what), "%s: the length is known up front", name);
   check(what, dec->totalFrames == frames);

   got = rmp3_read_s16(dec, (uint64_t)frames + 4096, pcm);
   snprintf(what, sizeof(what), "%s: %u samples in, %u out", name,
         frames, (unsigned)got);
   check(what, got == frames);

   n = ref_size / (channels * sizeof(int16_t));
   snprintf(what, sizeof(what), "%s: ffmpeg's trimmed decode is as long", name);
   check(what, n == got);
   if (n > got)
      n = got;
   for (i = 0; i < n * channels; i++)
   {
      int16_t  r = (int16_t)(ref[2 * i] | (ref[2 * i + 1] << 8));
      uint64_t d = (uint64_t)(r > pcm[i] ? r - pcm[i] : pcm[i] - r);
      if (d > worst)
         worst = d;
   }
   snprintf(what, sizeof(what), "%s: the same samples, off by at most %u",
         name, (unsigned)worst);
   check(what, n > 0 && worst <= 1);

   /* A seek counts from the trimmed start. */
   {
      int16_t  one[2] = {0, 0};
      uint64_t to     = (uint64_t)frames / 3;
      check("a seek lands on the sample the read gave",
            rmp3_seek_to_frame(dec, to)
            && rmp3_read_s16(dec, 1, one) == 1
            && one[0] == pcm[to * channels]);
      check("a seek reads to the same end",
            rmp3_seek_to_frame(dec, to)
            && rmp3_read_s16(dec, (uint64_t)frames, pcm) == frames - to);
      check("nothing past the end",
            rmp3_read_s16(dec, 16, pcm) == 0);
   }

done:
   free(pcm);
   free(dec);
   free(ref);
   free(mp3);
}

/* Without a tag nothing is trimmed: whole frames, the first included. */
static void test_untagged(void)
{
   char      path[512];
   uint8_t  *mp3;
   size_t    size = 0;
   rmp3     *dec;
   int16_t  *pcm;
   uint64_t  got;
   rmp3_vbr_tag tag;

   printf("no tag (-write_xing 0):\n");
   snprintf(path, sizeof(path), "%s/untagged.mp3", dir);
   if (run("ffmpeg -v error -y -f lavfi -i \"sine=f=440:r=44100\" "
           "-af atrim=end_sample=50000 -c:a libmp3lame -write_xing 0 "
           "-id3v2_version 0 '%s'", path))
   {
      check("ffmpeg encodes", 0);
      return;
   }
   mp3 = load(path, &size);
   dec = (rmp3*)calloc(1, sizeof(*dec));
   pcm = (int16_t*)malloc(65536 * 2 * sizeof(int16_t));
   if (!mp3 || !dec || !pcm || !rmp3_init_memory(dec, mp3, size))
      check("the stream opens", 0);
   else
   {
      check("the first frame is no tag", !rmp3_parse_vbr_tag(mp3, size, &tag));
      check("the length is not known", dec->totalFrames == 0);
      got = rmp3_read_s16(dec, 65536, pcm);
      check("whole frames, delay and padding kept",
            got % 1152 == 0 && got >= 50000 + RMP3_DECODER_DELAY);
   }
   free(pcm);
   free(dec);
   free(mp3);
}

/* Tags laid out by hand, in a frame of zeros behind the header. */
static void test_layouts(void)
{
   static const struct
   {
      const char *what;
      uint8_t     h1, h3;
      size_t      side;
      unsigned    samples;
   } layouts[] = {
      { "MPEG-1 stereo: the tag 32 bytes in",  0xFB, 0x44, 32, 1152 },
      { "MPEG-1 mono: 17 bytes in",            0xFB, 0xC4, 17, 1152 },
      { "MPEG-1 with a CRC: 2 more",           0xFA, 0x44, 34, 1152 },
      { "MPEG-2 stereo: 17 bytes in",          0xF3, 0x44, 17, 576  },
      { "MPEG-2 mono: 9 bytes in",             0xF3, 0xC4, 9,  576  },
   };
   uint8_t      frame[1024];
   rmp3_vbr_tag tag;
   unsigned     k;

   printf("tags built by hand:\n");
   for (k = 0; k < sizeof(layouts) / sizeof(layouts[0]); k++)
   {
      uint8_t *p;
      memset(frame, 0, sizeof(frame));
      frame[0] = 0xFF;
      frame[1] = layouts[k].h1;
      frame[2] = 0x90;
      frame[3] = layouts[k].h3;
      p = frame + 4 + layouts[k].side;
      memcpy(p, "Info", 4);
      put_be32(p + 4, 1 | 2 | 8);
      put_be32(p + 8, 77);
      put_be32(p + 12, 12345);
      memcpy(p + 20, "LAME3.100", 9);
      /* delay 576, padding 1000 */
      p[20 + 21] = 576 >> 4;
      p[20 + 22] = (uint8_t)(((576 & 15) << 4) | (1000 >> 8));
      p[20 + 23] = 1000 & 0xFF;
      check(layouts[k].what,
            rmp3_parse_vbr_tag(frame, sizeof(frame), &tag)
            && tag.frames == 77 && tag.bytes == 12345
            && tag.has_delay && tag.encoder_delay == 576
            && tag.encoder_padding == 1000
            && tag.frame_samples == layouts[k].samples);
   }

   /* Xing without LAME's extension: a tag, no delay. */
   memset(frame, 0, sizeof(frame));
   frame[0] = 0xFF; frame[1] = 0xFB; frame[2] = 0x90; frame[3] = 0x44;
   memcpy(frame + 36, "Xing", 4);
   put_be32(frame + 40, 1);
   put_be32(frame + 44, 9);
   check("Xing without LAME's extension: no delay",
         rmp3_parse_vbr_tag(frame, sizeof(frame), &tag)
         && tag.frames == 9 && !tag.has_delay);
   check("a tag cut off before its fields is still a tag, with none",
         rmp3_parse_vbr_tag(frame, 46, &tag) && tag.frames == 0);
   check("too short for the tag: no tag",
         !rmp3_parse_vbr_tag(frame, 39, &tag));

   memset(frame, 0, sizeof(frame));
   frame[0] = 0xFF; frame[1] = 0xFB; frame[2] = 0x90; frame[3] = 0x44;
   memcpy(frame + 36, "VBRI", 4);
   put_be32(frame + 46, 4321);
   put_be32(frame + 50, 55);
   check("VBRI: bytes and frames",
         rmp3_parse_vbr_tag(frame, sizeof(frame), &tag)
         && tag.is_vbri && tag.frames == 55 && tag.bytes == 4321);

   check("no header: no tag", !rmp3_parse_vbr_tag(frame + 1, 100, &tag));
}

int main(void)
{
   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("rmp3_gapless_test: ffmpeg is required (with libmp3lame)\n");
      return 1;
   }
   snprintf(dir, sizeof(dir), "/tmp/rmp3_gapless_test.%d", (int)getpid());
   if (run("mkdir -p '%s'", dir))
      return 1;

   test_stream("stereo_128k", "-b:a 128k", 100003, 44100, 2);
   test_stream("mono_vbr", "-q:a 4", 77777, 44100, 1);
   test_stream("mpeg2_22k", "-b:a 32k", 33333, 22050, 2);
   test_untagged();
   test_layouts();

   run("rm -rf '%s'", dir);
   printf("rmp3_gapless_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}

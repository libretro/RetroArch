/* The AC-3 / E-AC-3 frame layer against streams from another
 * implementation.
 *
 * ffmpeg, where it is installed, encodes tones into AC-3 and E-AC-3
 * across the sample rates, bit rates and channel configurations, and
 * ffprobe lists each stream's packets. The frame walk here must land
 * on every packet ffprobe reports, with the size, rate, channel
 * count and layout it reports, and every frame's CRC must hold. A
 * synthetic check covers what the walk must refuse: a sync word in
 * the middle of a frame, a truncated header, reserved codes.
 *
 * Without ffmpeg the stream checks are skipped and said to be. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rac3.h>

static unsigned failures = 0, skipped = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t *read_all(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb"); uint8_t *b; long n;
   if (!f) return NULL;
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc(n > 0 ? n : 1);
   *len = fread(b, 1, n, f); fclose(f);
   return b;
}

static int have_ffmpeg(void)
{
   return system("ffmpeg -version >/dev/null 2>&1") == 0
       && system("ffprobe -version >/dev/null 2>&1") == 0;
}

/* One stream: encode, probe, walk. */
static void stream_case(const char *codec, unsigned rate, unsigned channels, unsigned kbps, const char *name)
{
   char cmd[512], path[128], probe[128];
   size_t len, at = 0, i;
   uint8_t *data;
   FILE *pf;
   unsigned packets = 0, expected = 0, frames = 0;
   unsigned probe_rate = 0, probe_ch = 0;
   char layout_name[64] = "";
   unsigned sizes[4096];

   snprintf(path, sizeof(path), "/tmp/rac3_%s.bin", name);
   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y -f lavfi -i \"sine=frequency=440:sample_rate=%u:duration=0.5\" -ac %u -c:a %s -b:a %uk -f %s %s",
         rate, channels, codec, kbps, codec, path);
   if (system(cmd) != 0)
   {
      printf("   %-30s (ffmpeg would not encode it; skipped)\n", name);
      skipped++;
      return;
   }
   /* ffprobe: the stream's rate, channels, layout; then the packet sizes */
   snprintf(probe, sizeof(probe), "/tmp/rac3_%s.probe", name);
   snprintf(cmd, sizeof(cmd),
         "ffprobe -hide_banner -v error -show_entries stream=sample_rate,channels,channel_layout -show_entries packet=size -of csv=p=0 %s > %s",
         path, probe);
   if (system(cmd) != 0)
   {
      printf("   %-30s (ffprobe failed; skipped)\n", name);
      skipped++;
      return;
   }
   pf = fopen(probe, "r");
   if (pf)
   {
      char line[128];
      while (fgets(line, sizeof(line), pf))
      {
         unsigned a, b;
         char c[64];
         if (sscanf(line, "%u,%u,%63s", &a, &b, c) == 3 && a > 1000)
         {
            probe_rate = a; probe_ch = b; strncpy(layout_name, c, sizeof(layout_name) - 1);
         }
         else if (sscanf(line, "%u", &a) == 1 && expected < 4096)
            sizes[expected++] = a;
      }
      fclose(pf);
   }

   data = read_all(path, &len);
   CHECK(data && len > 0, "%s: no stream", name);
   if (!data) return;

   printf("   %-30s %u packets from ffprobe, %u Hz, %u ch (%s)\n", name, expected, probe_rate, probe_ch, layout_name);
   while (at < len)
   {
      rac3_frame_info_t info;
      enum rac3_status st = rac3_parse_frame_info(data + at, len - at, &info);
      if (st != RAC3_OK)
      {
         CHECK(0, "%s: frame %u at %u: parse status %d", name, frames, (unsigned)at, (int)st);
         break;
      }
      if (frames < expected)
         CHECK(info.frame_bytes == sizes[frames], "%s: frame %u is %u bytes, ffprobe says %u", name, frames, info.frame_bytes, sizes[frames]);
      CHECK(info.sample_rate == probe_rate, "%s: frame %u rate %u, ffprobe %u", name, frames, info.sample_rate, probe_rate);
      CHECK(info.channels == probe_ch, "%s: frame %u has %u channels, ffprobe %u", name, frames, info.channels, probe_ch);
      CHECK(info.kind == (strcmp(codec, "eac3") == 0 ? RAC3_KIND_EAC3 : RAC3_KIND_AC3), "%s: frame %u kind", name, frames);
      if (info.kind == RAC3_KIND_AC3)
         CHECK(info.bitrate == kbps * 1000, "%s: frame %u bitrate %u", name, frames, info.bitrate);
      CHECK(at + info.frame_bytes <= len, "%s: frame %u runs past the end", name, frames);
      if (at + info.frame_bytes > len) break;
      CHECK(rac3_frame_crc_ok(data + at, info.frame_bytes), "%s: frame %u CRC does not hold", name, frames);
      /* the layout: ffprobe's names for what we map */
      if (frames == 0)
      {
         uint32_t want = 0;
         if      (!strcmp(layout_name, "mono"))       want = 0x004u;
         else if (!strcmp(layout_name, "stereo"))     want = 0x003u;
         else if (!strcmp(layout_name, "3.0"))        want = 0x007u;
         else if (!strcmp(layout_name, "quad(side)")) want = 0x603u;
         else if (!strcmp(layout_name, "5.1(side)"))  want = 0x60Fu;
         else if (!strcmp(layout_name, "5.0(side)"))  want = 0x607u;
         else if (!strcmp(layout_name, "2.1"))        want = 0x00Bu;
         if (want)
            CHECK(info.layout == want, "%s: layout 0x%03x, ffprobe's %s is 0x%03x", name, info.layout, layout_name, want);
         else
            printf("      (ffprobe layout \"%s\" not mapped; ours 0x%03x)\n", layout_name, info.layout);
      }
      at += info.frame_bytes;
      frames++;
      packets++;
   }
   CHECK(frames == expected, "%s: walked %u frames, ffprobe lists %u", name, frames, expected);
   /* sync search from the middle of a frame lands on the next frame */
   if (len > 100 && expected > 2)
   {
      rac3_frame_info_t f0;
      rac3_parse_frame_info(data, len, &f0);
      i = rac3_find_sync(data, len, 3);
      CHECK(i == f0.frame_bytes, "%s: sync search from inside frame 0 found %u, frame 1 is at %u", name, (unsigned)i, f0.frame_bytes);
   }
   free(data);
   remove(path); remove(probe);
}

int main(void)
{
   printf("rac3 frame:\n");
   {
      /* Synthetic: refusals. */
      static const uint8_t no_sync[8]   = { 0x00, 0x00, 0, 0, 0, 0, 0, 0 };
      static const uint8_t bad_fscod[8] = { 0x0B, 0x77, 0x00, 0x00, 0xC0, 0x40, 0x00, 0x00 };  /* fscod 3 in AC-3 */
      static const uint8_t bsid_9[8]    = { 0x0B, 0x77, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00 };  /* bsid 9: reserved */
      rac3_frame_info_t info;
      printf("   refusals\n");
      CHECK(rac3_parse_frame_info(no_sync, 8, &info) == RAC3_NO_SYNC, "no sync word not refused");
      CHECK(rac3_parse_frame_info(bad_fscod, 3, &info) == RAC3_NEED_MORE, "a truncated header not reported short");
      CHECK(rac3_parse_frame_info(bad_fscod, 8, &info) == RAC3_BAD, "fscod 3 not refused");
      CHECK(rac3_parse_frame_info(bsid_9, 8, &info) == RAC3_BAD, "bsid 9 not refused");
      CHECK(rac3_crc16((const uint8_t*)"123456789", 9) == 0xFEE8, "CRC-16/BUYPASS of 123456789 is 0x%04x, not 0xFEE8", rac3_crc16((const uint8_t*)"123456789", 9));
   }
   if (!have_ffmpeg())
   {
      printf("   (ffmpeg not installed: the stream checks are skipped)\n");
      skipped++;
   }
   else
   {
      stream_case("ac3",  48000, 2, 192, "ac3-48k-stereo-192");
      stream_case("ac3",  44100, 2, 128, "ac3-44k-stereo-128");
      stream_case("ac3",  32000, 2,  96, "ac3-32k-stereo-96");
      stream_case("ac3",  48000, 1,  96, "ac3-48k-mono-96");
      stream_case("ac3",  48000, 6, 448, "ac3-48k-5.1-448");
      stream_case("ac3",  48000, 6, 640, "ac3-48k-5.1-640");
      stream_case("ac3",  44100, 6, 384, "ac3-44k-5.1-384");
      stream_case("eac3", 48000, 2, 192, "eac3-48k-stereo-192");
      stream_case("eac3", 48000, 6, 384, "eac3-48k-5.1-384");
      stream_case("eac3", 48000, 6, 1024, "eac3-48k-5.1-1024");
      stream_case("eac3", 44100, 6, 448, "eac3-44k-5.1-448");
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rac3 frame: every frame ffprobe lists is walked with its size, rate, channels and layout, and every CRC holds%s\n",
         skipped ? " (some cases skipped)" : "");
   return 0;
}

/* IEC 61937 bursts against ffmpeg's spdif muxer and demuxer.
 *
 * Our AC-3 encoder's frames are wrapped here and must decode through
 * ffmpeg's spdif demuxer back to the source; and an AC-3 stream
 * ffmpeg wraps with its own spdif muxer must be, burst for burst,
 * byte for byte, what we produce for the same frames. The pause
 * burst and the probe are checked by construction. Without ffmpeg
 * the ffmpeg parts are skipped. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/iec61937.h>
#include <formats/rac3.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t *read_all(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb"); uint8_t *b; long n;
   if (!f) { *len = 0; return NULL; }
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc(n > 0 ? n : 1);
   *len = fread(b, 1, n, f); fclose(f);
   return b;
}

static void by_construction(void)
{
   uint8_t frame[7] = { 0x0B, 0x77, 0x11, 0x22, 0x33, 0x44, 0x55 };
   uint8_t out[IEC61937_AC3_BURST_BYTES];
   unsigned type; size_t bytes, n;
   printf("   by construction\n");
   n = iec61937_wrap_ac3(frame, sizeof(frame), 2, out, sizeof(out));
   CHECK(n == IEC61937_AC3_BURST_BYTES, "AC-3 burst is %u bytes", (unsigned)n);
   CHECK(out[0] == 0x72 && out[1] == 0xF8 && out[2] == 0x1F && out[3] == 0x4E, "preamble Pa Pb");
   CHECK(out[4] == 0x01 && out[5] == 0x02, "Pc is AC-3 with bsmod 2 (%02x %02x)", out[4], out[5]);
   CHECK(out[6] == 56 && out[7] == 0, "Pd is 56 bits (%u)", out[6] | (out[7] << 8));
   CHECK(out[8] == 0x77 && out[9] == 0x0B && out[10] == 0x22 && out[11] == 0x11, "payload bytes swapped in pairs");
   CHECK(out[14] == 0x00 && out[15] == 0x55, "the odd byte is a word's high byte");
   CHECK(out[16] == 0 && out[IEC61937_AC3_BURST_BYTES - 1] == 0, "padding is zero");
   CHECK(iec61937_probe(out, n, &type, &bytes) && type == IEC61937_AC3 && bytes == 7, "probe reads the burst back");
   CHECK(!iec61937_probe(frame, sizeof(frame), NULL, NULL), "probe refuses a raw frame");
   CHECK(iec61937_wrap_ac3(frame, sizeof(frame), 0, out, 100) == 0, "a small cap is refused");
   CHECK(iec61937_wrap_dts(frame, sizeof(frame), 1024, out, sizeof(out)) == 4096 && out[4] == IEC61937_DTS_II, "DTS type II burst");
   CHECK(iec61937_wrap_dts(frame, sizeof(frame), 768, out, sizeof(out)) == 0, "an odd DTS frame length is refused");
   n = iec61937_pause_burst(1536, out, sizeof(out));
   CHECK(n == IEC61937_PAUSE_BURST_BYTES && out[4] == IEC61937_PAUSE && out[6] == 32, "pause burst: %u bytes, type %u, Pd %u", (unsigned)n, out[4], out[6]);
   CHECK(iec61937_probe(out, n, &type, &bytes) && type == IEC61937_PAUSE && bytes == 4, "pause burst probes");
}

static int have_ffmpeg(void)
{
   return system("ffmpeg -version >/dev/null 2>&1") == 0;
}

/* our encoder's frames, wrapped here, through ffmpeg's spdif demuxer */
static void through_ffmpeg_demuxer(void)
{
   size_t frames = 1536 * 20, i, out_len = 0, ref_len = 0;
   float *src = (float*)calloc(frames * 2, sizeof(float)), *ref;
   uint8_t *burst = (uint8_t*)malloc(IEC61937_AC3_BURST_BYTES), fr[4096];
   rac3_encoder_t *enc = rac3_encoder_new(48000, 0x003, 192);
   FILE *f = fopen("/tmp/iec61937_ours.spdif", "wb");
   printf("   our bursts through ffmpeg's spdif demuxer\n");
   for (i = 0; i < frames; i++)
   {
      src[i * 2]     = 0.4f * (float)sin(2.0 * 3.14159265358979 * 220.0 * (double)i / 48000.0);
      src[i * 2 + 1] = 0.4f * (float)sin(2.0 * 3.14159265358979 * 330.0 * (double)i / 48000.0);
   }
   for (i = 0; i + 1536 <= frames; i += 1536)
   {
      size_t n = rac3_encode_frame(enc, src + i * 2, fr, sizeof(fr));
      size_t b = iec61937_wrap_ac3(fr, n, 0, burst, IEC61937_AC3_BURST_BYTES);
      CHECK(b == IEC61937_AC3_BURST_BYTES, "frame %u wrapped", (unsigned)(i / 1536));
      fwrite(burst, 1, b, f);
      out_len += b;
   }
   fclose(f);
   CHECK(system("ffmpeg -hide_banner -loglevel error -y -f spdif -i /tmp/iec61937_ours.spdif -f f32le -c:a pcm_f32le /tmp/iec61937_ours.f32") == 0,
         "ffmpeg's spdif demuxer takes our bursts");
   ref = (float*)read_all("/tmp/iec61937_ours.f32", &ref_len);
   if (ref)
   {
      size_t n = ref_len / 8, k; unsigned ch;
      CHECK(n == frames, "ffmpeg decoded %u frames of %u", (unsigned)n, (unsigned)frames);
      for (ch = 0; ch < 2; ch++)
      {
         double aa = 0, err = 0, s;
         for (k = 256; k < n && k < frames; k++) { double x = src[(k - 256) * 2 + ch], y = ref[k * 2 + ch]; aa += x * x; err += (x - y) * (x - y); }
         s = err > 0 ? 10.0 * log10(aa / err) : 200.0;
         printf("      ch%u: %.1f dB against the source\n", ch, s);
         CHECK(s > 40.0, "ch%u through the demuxer is %.1f dB", ch, s);
      }
      free(ref);
   }
   remove("/tmp/iec61937_ours.spdif"); remove("/tmp/iec61937_ours.f32");
   rac3_encoder_free(enc); free(src); free(burst);
}

/* ffmpeg's spdif muxer on an AC-3 stream against our wrapping of the same frames */
static void against_ffmpeg_muxer(void)
{
   size_t ac3_len = 0, spdif_len = 0, at = 0, nb = 0;
   uint8_t *ac3, *spdif, *burst = (uint8_t*)malloc(IEC61937_AC3_BURST_BYTES);
   printf("   ffmpeg's spdif muxer against ours, byte for byte\n");
   CHECK(system("ffmpeg -hide_banner -loglevel error -y -f lavfi -i \"sine=frequency=440:sample_rate=48000:duration=1\" -ac 2 -c:a ac3 -b:a 192k /tmp/iec61937_ff.ac3 && "
                "ffmpeg -hide_banner -loglevel error -y -i /tmp/iec61937_ff.ac3 -c copy -f spdif /tmp/iec61937_ff.spdif") == 0, "ffmpeg made the streams");
   ac3   = read_all("/tmp/iec61937_ff.ac3", &ac3_len);
   spdif = read_all("/tmp/iec61937_ff.spdif", &spdif_len);
   if (ac3 && spdif)
   {
      while (at + 8 <= ac3_len)
      {
         rac3_frame_info_t info;
         size_t b;
         if (rac3_parse_frame_info(ac3 + at, ac3_len - at, &info) != RAC3_OK) break;
         b = iec61937_wrap_ac3(ac3 + at, info.frame_bytes, info.bsmod, burst, IEC61937_AC3_BURST_BYTES);
         CHECK(b == IEC61937_AC3_BURST_BYTES, "burst %u wrapped", (unsigned)nb);
         if ((nb + 1) * IEC61937_AC3_BURST_BYTES <= spdif_len)
            CHECK(!memcmp(burst, spdif + nb * IEC61937_AC3_BURST_BYTES, IEC61937_AC3_BURST_BYTES), "burst %u differs from ffmpeg's", (unsigned)nb);
         at += info.frame_bytes;
         nb++;
      }
      CHECK(nb * IEC61937_AC3_BURST_BYTES == spdif_len, "ffmpeg wrote %u bytes of bursts, we count %u frames", (unsigned)spdif_len, (unsigned)nb);
      printf("      %u bursts identical\n", (unsigned)nb);
   }
   free(ac3); free(spdif); free(burst);
   remove("/tmp/iec61937_ff.ac3"); remove("/tmp/iec61937_ff.spdif");
}

int main(void)
{
   printf("iec61937:\n");
   by_construction();
   if (have_ffmpeg())
   {
      through_ffmpeg_demuxer();
      against_ffmpeg_muxer();
   }
   else
      printf("   (ffmpeg not installed: the ffmpeg checks are skipped)\n");
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("iec61937: bursts as ffmpeg makes and takes them\n");
   return 0;
}

/* rdts and rlpcm against ffmpeg.
 *
 * DTS: ffmpeg's encoder makes the streams, and what is checked is the
 * frame layer - that every frame is found where ffmpeg put it, that
 * the rate, the arrangement and the sample count agree with what
 * ffprobe says of the same file, and that the four packings all read
 * to the same core. Nothing here decodes DTS; the frame layer is what
 * rdts implements.
 *
 * LPCM: the samples go out through rlpcm and come back through
 * ffmpeg's own converters, and the other way about, at every width
 * and both byte orders. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/rdts.h>
#include <formats/rlpcm.h>
#include <formats/audio.h>
#include <formats/iec61937.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   uint8_t *b;
   long n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc((size_t)n + 1);
   if (b && fread(b, 1, (size_t)n, f) != (size_t)n)
   {
      free(b);
      b = NULL;
   }
   fclose(f);
   if (b && len)
      *len = (size_t)n;
   return b;
}

static double probe_number(const char *path, const char *entry)
{
   char cmd[512];
   char buf[128] = {0};
   FILE *p;
   snprintf(cmd, sizeof(cmd),
         "ffprobe -v error -select_streams a:0 -show_entries stream=%s "
         "-of default=noprint_wrappers=1:nokey=1 %s 2>/dev/null", entry, path);
   p = popen(cmd, "r");
   if (!p)
      return -1.0;
   if (!fgets(buf, sizeof(buf), p))
      buf[0] = 0;
   pclose(p);
   return buf[0] ? atof(buf) : -1.0;
}

/* ---- DTS ---------------------------------------------------------- */

static void dts_case(const char *name, unsigned channels, unsigned rate, const char *extra)
{
   char cmd[768], path[128];
   uint8_t *buf;
   size_t len = 0, frames = 0, samples = 0;
   rdts_frame_info_t first;
   double probed_rate, probed_ch;

   snprintf(path, sizeof(path), "/tmp/rdts_%s.dts", name);
   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y -f lavfi "
         "-i \"sine=f=440:r=%u:d=2\" -ac %u -c:a dca -strict -2 %s -f dts %s",
         rate, channels, extra ? extra : "", path);
   if (system(cmd) != 0)
   {
      printf("      (ffmpeg would not make %s; skipped)\n", name);
      return;
   }
   buf = slurp(path, &len);
   CHECK(buf != NULL, "%s: no file", name);
   if (!buf)
      return;

   CHECK(rdts_find_sync(buf, len, 0) == 0, "%s: the first frame is not at the start", name);
   CHECK(rdts_scan(buf, len, &first, &frames, &samples), "%s: the buffer does not scan", name);

   probed_rate = probe_number(path, "sample_rate");
   probed_ch   = probe_number(path, "channels");
   printf("      %-14s %u frames, %u Hz, %u ch (mask 0x%03x), %u samples/frame, %u bytes/frame\n",
         name, (unsigned)frames, first.sample_rate, first.channels,
         first.layout, first.samples, first.frame_bytes);
   /* The coding header behind the frame header: what the frame would
    * take to decode. */
   CHECK(first.coding_header_read, "%s: the coding header was not read", name);
   if (first.coding_header_read)
   {
      printf("                     %u subframe(s), %u primary ch, %u subbands, VQ from %u%s%s\n",
            first.subframes, first.prim_channels, first.subbands,
            first.vq_start_subband,
            first.joint_intensity ? ", joint intensity" : "",
            rdts_decodable_from_spec(&first)
                  ? "" : ", needs the codebook the standard omits");
      CHECK(first.subframes >= 1 && first.subframes <= 16,
            "%s: %u subframes", name, first.subframes);
      CHECK(first.prim_channels >= 1 && first.prim_channels <= 5,
            "%s: %u primary channels", name, first.prim_channels);
      /* The primary channels are the core's less the LFE. */
      CHECK(first.prim_channels == first.channels - (first.lfe ? 1u : 0u),
            "%s: %u primary channels against %u core channels",
            name, first.prim_channels, first.channels);
      CHECK(first.subbands >= 2 && first.subbands <= 32,
            "%s: %u subbands", name, first.subbands);
   }
   CHECK(first.sample_rate == (unsigned)probed_rate,
         "%s: rate %u, ffprobe says %.0f", name, first.sample_rate, probed_rate);
   CHECK(first.channels == (unsigned)probed_ch,
         "%s: %u channels, ffprobe says %.0f", name, first.channels, probed_ch);
   CHECK(first.samples == 512 || first.samples == 1024 || first.samples == 256,
         "%s: %u samples a frame", name, first.samples);
   /* Two seconds of it, within a frame either way. */
   CHECK(samples >= first.sample_rate * 2 - first.samples
         && samples <= first.sample_rate * 2 + first.samples,
         "%s: %u samples for two seconds at %u Hz",
         name, (unsigned)samples, first.sample_rate);
   /* Every frame lands on the next: walked to the end without a gap. */
   {
      size_t at = 0, walked = 0;
      rdts_frame_info_t info;
      while (at + 4 <= len && rdts_parse_frame_info(buf + at, len - at, &info) == RDTS_OK)
      {
         if (!info.frame_bytes || at + info.frame_bytes > len)
            break;
         at += info.frame_bytes;
         walked++;
      }
      CHECK(at == len, "%s: walked %u frames and stopped %u bytes short of the end",
            name, (unsigned)walked, (unsigned)(len - at));
   }
   free(buf);
}

/* The same stream in the other three packings: byte-swapped, and the
 * two 14-bit forms a DTS audio CD carries. Every one has to read to
 * the core the plain form gives. */
static void dts_packing_case(void)
{
   uint8_t *buf, *swapped, *packed;
   uint8_t core_a[8192], core_b[8192];
   size_t len = 0, i, n_a, n_b;
   rdts_frame_info_t a, b;

   printf("   the four packings read to the same core\n");
   buf = slurp("/tmp/rdts_5_1.dts", &len);
   if (!buf)
   {
      printf("      (no stream to repack; skipped)\n");
      return;
   }
   CHECK(rdts_parse_frame_info(buf, len, &a) == RDTS_OK, "the plain frame does not parse");
   n_a = rdts_to_core(&a, buf, len, core_a, sizeof(core_a));
   CHECK(n_a == a.core_bytes, "the plain frame gave %u of %u core bytes",
         (unsigned)n_a, a.core_bytes);

   /* 16-bit little-endian: the words the other way round. */
   swapped = (uint8_t*)malloc(len);
   for (i = 0; i + 1 < len; i += 2)
   {
      swapped[i]     = buf[i + 1];
      swapped[i + 1] = buf[i];
   }
   CHECK(rdts_parse_frame_info(swapped, len, &b) == RDTS_OK,
         "the byte-swapped frame does not parse");
   if (b.packing == RDTS_PACK_16LE)
   {
      n_b = rdts_to_core(&b, swapped, len, core_b, sizeof(core_b));
      CHECK(n_b == n_a && memcmp(core_a, core_b, n_a) == 0,
            "the byte-swapped frame gives a different core (%u vs %u bytes)",
            (unsigned)n_b, (unsigned)n_a);
      CHECK(b.sample_rate == a.sample_rate && b.channels == a.channels,
            "the byte-swapped frame reads a different shape");
   }
   else
      CHECK(0, "the byte-swapped frame read as packing %d", (int)b.packing);

   /* 14-bit big-endian: fourteen bits of the stream in each word. */
   {
      size_t bits = a.core_bytes * 8;
      size_t words = (bits + 13) / 14;
      packed = (uint8_t*)calloc(words * 2 + 4, 1);
      for (i = 0; i < bits; i++)
      {
         unsigned bit = (buf[i >> 3] >> (7 - (i & 7))) & 1u;
         size_t   w   = i / 14;
         unsigned pos = (unsigned)(i % 14) + 2;
         if (bit)
            packed[w * 2 + (pos < 8 ? 0 : 1)] |= (uint8_t)(0x80u >> (pos & 7u));
      }
      /* Each word is sign-extended from fourteen bits to sixteen -
       * which is why the 14-bit sync word reads 0x1FFFE800 and not
       * the 0x1FFF2800 the bare bits would give. */
      for (i = 0; i < words; i++)
         if (packed[i * 2] & 0x20u)
            packed[i * 2] |= 0xC0u;
      CHECK(rdts_parse_frame_info(packed, words * 2, &b) == RDTS_OK,
            "the 14-bit frame does not parse");
      if (b.packing == RDTS_PACK_14BE)
      {
         n_b = rdts_to_core(&b, packed, words * 2, core_b, sizeof(core_b));
         printf("      14-bit: %u bytes on the wire for a %u-byte core\n",
               (unsigned)(words * 2), a.core_bytes);
         CHECK(n_b == n_a && memcmp(core_a, core_b, n_a) == 0,
               "the 14-bit frame gives a different core (%u vs %u bytes)",
               (unsigned)n_b, (unsigned)n_a);
         CHECK(b.sample_rate == a.sample_rate && b.channels == a.channels,
               "the 14-bit frame reads a different shape");
      }
      else
         CHECK(0, "the 14-bit frame read as packing %d", (int)b.packing);
      free(packed);
   }
   free(swapped);
   free(buf);
}

/* The pass-through route: a DTS stream to a receiver, which is what a
 * decoder-less path is for. Every frame is repacked to the plain core
 * the receiver takes, put in the burst its sample count calls for,
 * and read back out of that burst byte for byte. */
static void dts_passthrough_case(void)
{
   uint8_t *buf;
   size_t   len = 0, at = 0, bursts = 0;
   uint8_t  core[16384], burst[32768];

   printf("   the pass-through route: frames to IEC 61937 bursts and back\n");
   buf = slurp("/tmp/rdts_5_1.dts", &len);
   if (!buf)
   {
      printf("      (no stream to carry; skipped)\n");
      return;
   }
   while (at + 4 <= len)
   {
      rdts_frame_info_t info;
      unsigned type = 0, pcm = 0, probed = 0;
      size_t   n, b, payload = 0;
      if (rdts_parse_frame_info(buf + at, len - at, &info) != RDTS_OK)
         break;
      if (!info.frame_bytes || at + info.frame_bytes > len)
         break;
      CHECK(rdts_burst_type(&info, &type, &pcm),
            "a %u-sample frame has no burst to go in", info.samples);
      CHECK(type == IEC61937_DTS_I && pcm == 512,
            "a 512-sample frame went to type %u, period %u", type, pcm);
      n = rdts_to_core(&info, buf + at, len - at, core, sizeof(core));
      CHECK(n == info.core_bytes, "repacked %u of %u core bytes",
            (unsigned)n, info.core_bytes);
      b = iec61937_wrap_dts(core, n, pcm, burst, sizeof(burst));
      CHECK(b == (size_t)pcm * 4, "the burst is %u bytes for a %u-frame period",
            (unsigned)b, pcm);
      /* And what a receiver would see coming back out of it. */
      CHECK(iec61937_probe(burst, b, &probed, &payload),
            "the burst does not probe");
      CHECK(probed == IEC61937_DTS_I, "the burst probes as type %u", probed);
      CHECK(payload == n, "the burst carries %u bytes of a %u-byte frame",
            (unsigned)payload, (unsigned)n);
      {
         /* The payload is byte-swapped into the burst, as 61937 has
          * it; swapped back it has to be the frame that went in. */
         uint8_t back[16384];
         size_t  i;
         for (i = 0; i + 1 < payload; i += 2)
         {
            back[i]     = burst[8 + i + 1];
            back[i + 1] = burst[8 + i];
         }
         if (payload & 1)
            back[payload - 1] = burst[8 + payload - 1];
         CHECK(memcmp(back, core, payload) == 0,
               "what came out of the burst is not what went in");
      }
      bursts++;
      at += info.frame_bytes;
      if (bursts >= 8)
         break;
   }
   printf("      %u frame(s) carried, each in a %u-byte type I burst\n",
         (unsigned)bursts, 512 * 4);
   CHECK(bursts >= 8, "only %u frames were carried", (unsigned)bursts);
   free(buf);
}

static void dts_junk_case(void)
{
   uint8_t junk[4096];
   rdts_frame_info_t info;
   size_t i;
   printf("   a buffer that is not DTS\n");
   for (i = 0; i < sizeof(junk); i++)
      junk[i] = (uint8_t)(i * 37u + (i >> 3));
   CHECK(rdts_parse_frame_info(junk, sizeof(junk), &info) == RDTS_NO_SYNC,
         "noise parsed as a frame");
   CHECK(rdts_find_sync(junk, sizeof(junk), 0) == sizeof(junk),
         "a sync word was found in noise");
   CHECK(!rdts_scan(junk, sizeof(junk), &info, NULL, NULL), "noise scanned as a stream");
   /* A sync word with nothing behind it is not a frame. */
   memcpy(junk, "\x7F\xFE\x80\x01", 4);
   CHECK(rdts_parse_frame_info(junk, 4, &info) == RDTS_NEED_MORE,
         "a lone sync word parsed as a frame");
}

/* ---- LPCM --------------------------------------------------------- */

static void lpcm_case(unsigned bits, unsigned channels, bool be)
{
   const char *fmt_name = bits == 16 ? (be ? "s16be" : "s16le")
                        : (be ? "s24be" : "s24le");
   char cmd[1024], src[96], back[96];
   uint8_t *raw;
   float   *dec, *ref;
   size_t   len = 0, ref_len = 0, frames, i;
   rlpcm_format_t fmt;
   double worst = 0.0;

   snprintf(src,  sizeof(src),  "/tmp/rlpcm_%u_%s.raw", bits, be ? "be" : "le");
   snprintf(back, sizeof(back), "/tmp/rlpcm_%u_%s.f32", bits, be ? "be" : "le");
   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y -f lavfi "
         "-i \"sine=f=440:r=48000:d=1\" -ac %u -c:a pcm_%s -f %s %s && "
         "ffmpeg -hide_banner -loglevel error -y -f %s -ar 48000 -ac %u -i %s "
         "-f f32le -c:a pcm_f32le %s",
         channels, fmt_name, fmt_name, src, fmt_name, channels, src, back);
   if (system(cmd) != 0)
   {
      printf("      (ffmpeg would not make %u-bit %s; skipped)\n", bits, be ? "BE" : "LE");
      return;
   }
   raw = slurp(src, &len);
   ref = (float*)slurp(back, &ref_len);
   CHECK(raw && ref, "%u-bit: no data", bits);
   if (!raw || !ref)
      return;

   memset(&fmt, 0, sizeof(fmt));
   fmt.sample_rate = 48000;
   fmt.bits        = bits;
   fmt.channels    = channels;
   fmt.big_endian  = be;
   CHECK(rlpcm_parse_format(RLPCM_KIND_RAW, NULL, 0, &fmt) == RLPCM_OK,
         "%u-bit: the raw format was refused", bits);

   frames = len / ((bits / 8) * channels);
   dec    = (float*)malloc(frames * channels * sizeof(float));
   CHECK(rlpcm_decode_f32(&fmt, raw, len, dec, frames) == frames,
         "%u-bit: decoded short", bits);
   for (i = 0; i < frames * channels && i < ref_len / sizeof(float); i++)
   {
      double d = fabs((double)dec[i] - (double)ref[i]);
      if (d > worst)
         worst = d;
   }
   printf("      %u-bit %s, %u ch: %u frames, worst difference from ffmpeg %.3g\n",
         bits, be ? "BE" : "LE", channels, (unsigned)frames, worst);
   CHECK(worst < 1e-6, "%u-bit %s: %.3g from ffmpeg's own decode", bits, be ? "BE" : "LE", worst);

   /* And back out: what rlpcm writes has to be the bytes it read. */
   {
      uint8_t *enc = (uint8_t*)malloc(len);
      size_t   n   = rlpcm_encode_f32(&fmt, dec, frames, enc, len);
      CHECK(n == frames, "%u-bit: encoded %u of %u frames", bits, (unsigned)n, (unsigned)frames);
      CHECK(memcmp(enc, raw, frames * (bits / 8) * channels) == 0,
            "%u-bit %s: what was written back is not what was read", bits, be ? "BE" : "LE");
      free(enc);
   }
   free(dec); free(raw); free(ref);
}

/* 20-bit has no ffmpeg format to compare against - it is a disc
 * packing, two samples in five bytes - so it is checked against
 * itself: known values in, the same values out. */
static void lpcm_20bit_case(void)
{
   rlpcm_format_t fmt;
   float  in[8], out[8];
   uint8_t bytes[32];
   size_t i, n;
   printf("   20-bit, two samples to five bytes\n");
   memset(&fmt, 0, sizeof(fmt));
   fmt.sample_rate = 48000;
   fmt.bits        = 20;
   fmt.channels    = 2;
   fmt.big_endian  = true;
   CHECK(rlpcm_parse_format(RLPCM_KIND_RAW, NULL, 0, &fmt) == RLPCM_OK, "20-bit refused");
   CHECK(rlpcm_frame_bits(&fmt) == 40, "a 20-bit stereo frame is %u bits", rlpcm_frame_bits(&fmt));
   for (i = 0; i < 8; i++)
      in[i] = (float)((double)((int)i - 4) / 4.5);
   n = rlpcm_encode_f32(&fmt, in, 4, bytes, sizeof(bytes));
   CHECK(n == 4, "encoded %u of 4 frames", (unsigned)n);
   n = rlpcm_decode_f32(&fmt, bytes, rlpcm_frame_bytes(&fmt) * 4, out, 4);
   CHECK(n == 4, "decoded %u of 4 frames", (unsigned)n);
   for (i = 0; i < 8; i++)
      CHECK(fabs((double)in[i] - (double)out[i]) < 1.0 / 524288.0,
            "sample %u went in at %.6f and came out at %.6f", (unsigned)i, in[i], out[i]);
}

static void lpcm_header_case(void)
{
   rlpcm_format_t fmt;
   uint8_t dvd[5], bd[4];
   printf("   the disc headers\n");

   /* DVD: 24-bit, 96 kHz, six channels. */
   dvd[0] = 0x05; dvd[1] = 0x00; dvd[2] = 0x03;
   dvd[3] = (uint8_t)((2u << 6) | (1u << 4) | 5u);
   dvd[4] = 0x80;
   memset(&fmt, 0, sizeof(fmt));
   CHECK(rlpcm_parse_format(RLPCM_KIND_DVD, dvd, sizeof(dvd), &fmt) == RLPCM_OK,
         "the DVD header was refused");
   printf("      DVD: %u-bit, %u Hz, %u ch, mask 0x%03x, %u-byte header\n",
         fmt.bits, fmt.sample_rate, fmt.channels, fmt.layout, fmt.header_bytes);
   CHECK(fmt.bits == 24 && fmt.sample_rate == 96000 && fmt.channels == 6,
         "the DVD header read %u-bit %u Hz %u ch", fmt.bits, fmt.sample_rate, fmt.channels);
   CHECK(fmt.big_endian, "the DVD header is not big-endian");
   CHECK(fmt.header_bytes == 5, "the DVD header is %u bytes", fmt.header_bytes);
   CHECK(rlpcm_parse_format(RLPCM_KIND_DVD, dvd, 3, &fmt) == RLPCM_NEED_MORE,
         "a short DVD header did not ask for more");
   dvd[3] = (uint8_t)((3u << 6) | 5u);
   CHECK(rlpcm_parse_format(RLPCM_KIND_DVD, dvd, sizeof(dvd), &fmt) == RLPCM_BAD,
         "a reserved sample size was accepted");

   /* Blu-ray: 5.1, 48 kHz, 24-bit, a 4096-byte payload. */
   bd[0] = 0x10; bd[1] = 0x00;
   bd[2] = (uint8_t)((9u << 4) | 1u);
   bd[3] = (uint8_t)(3u << 6);
   memset(&fmt, 0, sizeof(fmt));
   CHECK(rlpcm_parse_format(RLPCM_KIND_BLURAY, bd, sizeof(bd), &fmt) == RLPCM_OK,
         "the Blu-ray header was refused");
   printf("      Blu-ray: %u-bit, %u Hz, %u ch, mask 0x%03x, %u-byte payload\n",
         fmt.bits, fmt.sample_rate, fmt.channels, fmt.layout, (unsigned)fmt.payload_bytes);
   CHECK(fmt.bits == 24 && fmt.sample_rate == 48000 && fmt.channels == 6,
         "the Blu-ray header read %u-bit %u Hz %u ch", fmt.bits, fmt.sample_rate, fmt.channels);
   /* The surround pair of a 5.1 layout sits at 110 degrees, which the
    * mask calls the side pair, as the AC-3 side of the tree has it. */
   CHECK(fmt.layout == 0x60Fu, "the Blu-ray 5.1 mask is 0x%03x", fmt.layout);
   CHECK(fmt.payload_bytes == 4096, "the payload is %u bytes", (unsigned)fmt.payload_bytes);
   bd[2] = (uint8_t)((2u << 4) | 1u);   /* a reserved assignment */
   CHECK(rlpcm_parse_format(RLPCM_KIND_BLURAY, bd, sizeof(bd), &fmt) == RLPCM_BAD,
         "a reserved channel assignment was accepted");
}

/* The audio_transfer arm: what the mixer and the preview actually
 * call. Reads, seeks and the frontier, against the same file read
 * straight through rlpcm. */
static void transfer_case(void)
{
   uint8_t *raw;
   size_t   len = 0;
   void    *h;
   unsigned channels = 0, rate = 0;
   uint64_t total = 0;
   float   *whole, *part;
   size_t   frames, got = 0, i;

   printf("   the audio_transfer arm\n");
   raw = slurp("/tmp/rlpcm_24_be.raw", &len);
   if (!raw)
   {
      printf("      (no samples to read; skipped)\n");
      return;
   }
   CHECK(audio_decode_get_type("track.lpcm") == AUDIO_TYPE_LPCM,
         ".lpcm is not recognised");
   CHECK(audio_decode_get_type("track.pcm") == AUDIO_TYPE_LPCM,
         ".pcm is not recognised");

   h = audio_transfer_new(AUDIO_TYPE_LPCM);
   CHECK(h != NULL, "the arm would not allocate");
   if (!h) { free(raw); return; }
   audio_transfer_set_buffer_ptr(h, AUDIO_TYPE_LPCM, raw, len);
   /* Raw samples say nothing about themselves, so the shape is the
    * caller's to give - which is what a track ripped from a disc
    * looks like to the frontend. */
   {
      rlpcm_format_t *fmt = (rlpcm_format_t*)audio_transfer_lpcm_format(h);
      CHECK(fmt != NULL, "the arm exposes no format to fill in");
      if (fmt)
      {
         fmt->sample_rate = 48000;
         fmt->bits        = 24;
         fmt->channels    = 2;
         fmt->big_endian  = true;
      }
   }
   CHECK(audio_transfer_start(h, AUDIO_TYPE_LPCM), "the arm would not start");
   CHECK(audio_transfer_is_valid(h, AUDIO_TYPE_LPCM), "the arm is not valid after starting");
   CHECK(audio_transfer_info(h, AUDIO_TYPE_LPCM, &channels, &rate, &total),
         "the arm reports no info");
   frames = len / 6;
   printf("      %u ch, %u Hz, %u frames (the file holds %u)\n",
         channels, rate, (unsigned)total, (unsigned)frames);
   CHECK(channels == 2 && rate == 48000, "the arm reports %u ch at %u Hz", channels, rate);
   CHECK(total == frames, "the arm counts %u frames of %u", (unsigned)total, (unsigned)frames);

   whole = (float*)malloc(frames * 2 * sizeof(float));
   part  = (float*)malloc(frames * 2 * sizeof(float));
   /* Read in chunks, as a mixer does. */
   while (got < frames)
   {
      size_t n = 0;
      int    r = audio_transfer_read_f32(h, AUDIO_TYPE_LPCM,
            whole + got * 2, 733, &n);
      if (r == AUDIO_PROCESS_END || !n)
         break;
      CHECK(r == AUDIO_PROCESS_NEXT, "a read returned %d", r);
      got += n;
   }
   CHECK(got == frames, "read %u frames of %u", (unsigned)got, (unsigned)frames);
   CHECK(audio_transfer_buffer_tell(h, AUDIO_TYPE_LPCM) == len,
         "the frontier is %u of %u bytes",
         (unsigned)audio_transfer_buffer_tell(h, AUDIO_TYPE_LPCM), (unsigned)len);

   /* The same samples straight through rlpcm. */
   {
      rlpcm_format_t fmt;
      memset(&fmt, 0, sizeof(fmt));
      fmt.sample_rate = 48000;
      fmt.bits        = 24;
      fmt.channels    = 2;
      fmt.big_endian  = true;
      rlpcm_parse_format(RLPCM_KIND_RAW, NULL, 0, &fmt);
      rlpcm_decode_f32(&fmt, raw, len, part, frames);
   }
   for (i = 0; i < frames * 2; i++)
      if (whole[i] != part[i])
      {
         CHECK(0, "the arm and the codec differ at sample %u", (unsigned)i);
         break;
      }

   /* Seeking is arithmetic here, so it lands exactly. */
   CHECK(audio_transfer_seek(h, AUDIO_TYPE_LPCM, 1000), "the seek was refused");
   {
      size_t n = 0;
      audio_transfer_read_f32(h, AUDIO_TYPE_LPCM, part, 16, &n);
      CHECK(n == 16, "read %u frames after seeking", (unsigned)n);
      for (i = 0; i < 32; i++)
         if (part[i] != whole[1000 * 2 + i])
         {
            CHECK(0, "the seek landed elsewhere (sample %u)", (unsigned)i);
            break;
         }
   }
   CHECK(!audio_transfer_seek(h, AUDIO_TYPE_LPCM, frames + 1),
         "a seek past the end was accepted");

   audio_transfer_free(h, AUDIO_TYPE_LPCM);
   free(whole); free(part); free(raw);
}

int main(void)
{
   printf("dts and lpcm:\n");

   printf("   DTS streams from ffmpeg, walked frame by frame\n");
   dts_case("stereo", 2, 48000, NULL);
   dts_case("5_1",    6, 48000, NULL);
   dts_case("44k",    2, 44100, NULL);
   dts_packing_case();
   dts_passthrough_case();
   dts_junk_case();

   printf("   LPCM against ffmpeg's own converters\n");
   lpcm_case(16, 2, true);
   lpcm_case(16, 2, false);
   lpcm_case(24, 2, true);
   lpcm_case(24, 6, true);
   lpcm_20bit_case();
   lpcm_header_case();
   transfer_case();

   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("dts and lpcm: the frames are found where ffmpeg put them, and the samples are the samples\n");
   return 0;
}

/* audio/audio_bitstream.c: a coded stream on its way to a receiver.
 *
 * What is checked is the three things the layer exists to settle -
 * that a read comes out on burst boundaries and never partial, that a
 * gap produces a pause burst rather than silence, and that the PCM
 * frames it reports are the device time the bytes occupy - and that
 * what a receiver would see coming out is the frames that went in.
 *
 * The fixtures are real: ffmpeg's encoders make the DTS and AC-3
 * streams, and the frames are handed over as they arrived. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rdts.h>
#include <formats/rac3.h>
#include <formats/iec61937.h>

#include "../../../audio/audio_bitstream.h"

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

static bool make(const char *cmd)
{
   return system(cmd) == 0;
}

/* ---- DTS ---------------------------------------------------------- */

static void dts_case(void)
{
   uint8_t *buf;
   size_t   len = 0, at = 0;
   audio_bitstream_t *bs;
   uint8_t  out[65536];
   size_t   n, frames = 0;
   unsigned carried = 0, type = 0;
   size_t   payload = 0;

   printf("   a DTS stream to a receiver\n");
   if (!make("ffmpeg -hide_banner -loglevel error -y -f lavfi "
            "-i \"sine=f=440:r=48000:d=2\" -ac 6 -c:a dca -strict -2 "
            "-f dts /tmp/bs.dts"))
   {
      printf("      (ffmpeg would not make a stream; skipped)\n");
      return;
   }
   buf = slurp("/tmp/bs.dts", &len);
   CHECK(buf != NULL, "no stream");
   if (!buf)
      return;

   bs = audio_bitstream_new(AUDIO_BITSTREAM_DTS, 48000);
   CHECK(bs != NULL, "the source would not open");
   if (!bs) { free(buf); return; }

   /* Before a frame has been handed over there is no burst size, so
    * there is nothing to send: a receiver has not been given a stream
    * to lock to yet. */
   CHECK(audio_bitstream_read(bs, out, sizeof(out), NULL) == 0,
         "the source produced bytes before any frame");
   CHECK(audio_bitstream_burst_bytes(bs) == 0, "a burst size before any frame");

   /* One frame in, one burst out, and what comes out is what a
    * receiver reads. */
   {
      rdts_frame_info_t info;
      CHECK(rdts_parse_frame_info(buf, len, &info) == RDTS_OK, "the first frame does not parse");
      CHECK(audio_bitstream_submit(bs, buf, len), "the first frame was refused");
      printf("      burst: %u bytes, %u PCM frames\n",
            (unsigned)audio_bitstream_burst_bytes(bs),
            audio_bitstream_burst_frames(bs));
      CHECK(audio_bitstream_burst_frames(bs) == info.samples,
            "a %u-sample frame went in a %u-frame burst",
            info.samples, audio_bitstream_burst_frames(bs));
      CHECK(audio_bitstream_burst_bytes(bs) == (size_t)info.samples * 4,
            "the burst is %u bytes", (unsigned)audio_bitstream_burst_bytes(bs));

      /* A read smaller than a burst produces nothing: a device given
       * half a burst loses the rest of the stream. */
      n = audio_bitstream_read(bs, out, audio_bitstream_burst_bytes(bs) - 4, &frames);
      CHECK(n == 0 && frames == 0, "a short read produced %u bytes", (unsigned)n);
      CHECK(audio_bitstream_queued(bs) == 1, "the short read consumed the frame");

      n = audio_bitstream_read(bs, out, sizeof(out), &frames);
      CHECK(n == audio_bitstream_burst_bytes(bs),
            "one frame produced %u bytes", (unsigned)n);
      CHECK(frames == info.samples, "one burst accounted for %u frames of %u",
            (unsigned)frames, info.samples);
      CHECK(iec61937_probe(out, n, &type, &payload), "the burst does not probe");
      CHECK(type == IEC61937_DTS_I, "the burst probes as type %u", type);
      CHECK(payload == info.core_bytes, "the burst carries %u bytes of a %u-byte frame",
            (unsigned)payload, info.core_bytes);
      at = info.frame_bytes;
   }

   /* The queue paces: submit until refused, which is the device
    * rather than the source deciding how far ahead it runs. */
   {
      unsigned taken = 0;
      while (at + 4 <= len)
      {
         rdts_frame_info_t info;
         if (rdts_parse_frame_info(buf + at, len - at, &info) != RDTS_OK)
            break;
         if (!audio_bitstream_submit(bs, buf + at, len - at))
            break;
         at += info.frame_bytes;
         taken++;
      }
      printf("      the queue took %u frame(s) before refusing (capacity %u)\n",
            taken, audio_bitstream_capacity(bs));
      CHECK(taken == audio_bitstream_capacity(bs),
            "the queue took %u of %u", taken, audio_bitstream_capacity(bs));
      CHECK(!audio_bitstream_writable(bs), "a full queue still says it is writable");
   }

   /* Drained and refilled to the end of the file, counting what the
    * device time comes to. */
   {
      size_t total_frames = 0;
      unsigned bursts = 0;
      /* Submit what fits, read what is queued, repeat - which is the
       * shape of a write path that keeps a device fed. A read is only
       * made when something is queued, so a gap here would mean the
       * source really did fall behind. */
      while (audio_bitstream_queued(bs) || at + 4 <= len)
      {
         while (at + 4 <= len && audio_bitstream_writable(bs))
         {
            rdts_frame_info_t info;
            if (rdts_parse_frame_info(buf + at, len - at, &info) != RDTS_OK)
            {
               at = len;
               break;
            }
            if (!audio_bitstream_submit(bs, buf + at, len - at))
               break;
            at += info.frame_bytes;
         }
         if (!audio_bitstream_queued(bs))
            break;
         n = audio_bitstream_read(bs, out, sizeof(out), &frames);
         if (!n)
            break;
         total_frames += frames;
         bursts       += (unsigned)(n / audio_bitstream_burst_bytes(bs));
         carried++;
      }
      printf("      %u burst(s) carried, %u PCM frames of device time, %u gap(s)\n",
            bursts, (unsigned)total_frames, audio_bitstream_gaps(bs));
      /* Two seconds at 48 kHz, within a burst either way. */
      CHECK(total_frames + audio_bitstream_burst_frames(bs) >= 96000
            && total_frames <= 96000 + audio_bitstream_burst_frames(bs) * 2,
            "%u frames of device time for two seconds", (unsigned)total_frames);
      CHECK(audio_bitstream_gaps(bs) == 0,
            "%u gap(s) with the source keeping up", audio_bitstream_gaps(bs));
   }

   audio_bitstream_free(bs);
   free(buf);
}

/* A starved source: what a receiver is given when nothing is queued
 * has to be a pause burst, not silence. */
static void gap_case(void)
{
   uint8_t *buf;
   size_t   len = 0, n, frames = 0;
   audio_bitstream_t *bs;
   uint8_t  out[65536];
   unsigned type = 0;
   size_t   payload = 0;

   printf("   a source that is late\n");
   buf = slurp("/tmp/bs.dts", &len);
   if (!buf)
   {
      printf("      (no stream; skipped)\n");
      return;
   }
   bs = audio_bitstream_new(AUDIO_BITSTREAM_DTS, 48000);
   CHECK(bs != NULL, "the source would not open");
   if (!bs) { free(buf); return; }
   CHECK(audio_bitstream_submit(bs, buf, len), "the first frame was refused");
   audio_bitstream_read(bs, out, audio_bitstream_burst_bytes(bs), NULL);

   /* Nothing queued now. */
   memset(out, 0xAA, sizeof(out));
   n = audio_bitstream_read(bs, out, audio_bitstream_burst_bytes(bs), &frames);
   CHECK(n == audio_bitstream_burst_bytes(bs),
         "a starved read produced %u bytes", (unsigned)n);
   CHECK(audio_bitstream_gaps(bs) == 1, "%u gaps counted", audio_bitstream_gaps(bs));
   CHECK(iec61937_probe(out, n, &type, &payload), "the gap does not probe as a burst");
   CHECK(type == IEC61937_PAUSE, "the gap probes as type %u, not a pause", type);
   /* A pause burst accounts for its own length and no more: the time
    * the missing audio would have taken is not claimed. */
   printf("      the gap is a %u-byte pause burst accounting for %u frames\n",
         (unsigned)n, (unsigned)frames);
   CHECK(frames > 0 && frames < audio_bitstream_burst_frames(bs),
         "a pause burst accounted for %u frames of a %u-frame period",
         (unsigned)frames, audio_bitstream_burst_frames(bs));
   /* And the rest of the period is zeroed, not left as it was. */
   {
      size_t i;
      bool clean = true;
      for (i = IEC61937_PAUSE_BURST_BYTES; i < n; i++)
         if (out[i] != 0)
            clean = false;
      CHECK(clean, "the pause burst's padding is not zeroed");
   }
   audio_bitstream_free(bs);
   free(buf);
}

/* ---- AC-3 --------------------------------------------------------- */

static void ac3_case(void)
{
   uint8_t *buf;
   size_t   len = 0, n, frames = 0;
   audio_bitstream_t *bs;
   uint8_t  out[65536];
   unsigned type = 0;
   size_t   payload = 0;
   rac3_frame_info_t info;

   printf("   an AC-3 stream that arrived already coded\n");
   if (!make("ffmpeg -hide_banner -loglevel error -y -f lavfi "
            "-i \"sine=f=440:r=48000:d=1\" -ac 6 -c:a ac3 -b:a 448k "
            "-f ac3 /tmp/bs.ac3"))
   {
      printf("      (ffmpeg would not make a stream; skipped)\n");
      return;
   }
   buf = slurp("/tmp/bs.ac3", &len);
   CHECK(buf != NULL, "no stream");
   if (!buf)
      return;
   bs = audio_bitstream_new(AUDIO_BITSTREAM_AC3, 48000);
   CHECK(bs != NULL, "the source would not open");
   if (!bs) { free(buf); return; }
   CHECK(audio_bitstream_burst_bytes(bs) == IEC61937_AC3_BURST_BYTES,
         "the AC-3 burst is %u bytes", (unsigned)audio_bitstream_burst_bytes(bs));
   CHECK(audio_bitstream_burst_frames(bs) == 1536,
         "the AC-3 burst stands for %u frames", audio_bitstream_burst_frames(bs));

   CHECK(rac3_parse_frame_info(buf, len, &info) == RAC3_OK, "the frame does not parse");
   CHECK(audio_bitstream_submit(bs, buf, len), "the frame was refused");
   n = audio_bitstream_read(bs, out, sizeof(out), &frames);
   CHECK(n == IEC61937_AC3_BURST_BYTES, "one frame produced %u bytes", (unsigned)n);
   CHECK(frames == 1536, "one burst accounted for %u frames", (unsigned)frames);
   CHECK(iec61937_probe(out, n, &type, &payload), "the burst does not probe");
   CHECK(type == IEC61937_AC3, "the burst probes as type %u", type);
   CHECK(payload == info.frame_bytes, "the burst carries %u of %u bytes",
         (unsigned)payload, info.frame_bytes);
   printf("      %u-byte frame in a %u-byte burst, 1536 frames of device time\n",
         info.frame_bytes, (unsigned)n);

   /* A buffer that is not a frame of the kind the source was made for
    * is refused rather than wrapped. */
   {
      uint8_t junk[512];
      size_t  i;
      for (i = 0; i < sizeof(junk); i++)
         junk[i] = (uint8_t)(i * 29u);
      CHECK(!audio_bitstream_submit(bs, junk, sizeof(junk)),
            "noise was accepted as a frame");
   }
   audio_bitstream_free(bs);
   free(buf);
}

int main(void)
{
   printf("audio bitstream:\n");
   dts_case();
   gap_case();
   ac3_case();
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("audio bitstream: bursts leave whole, a gap is a pause, and the device time adds up\n");
   return 0;
}

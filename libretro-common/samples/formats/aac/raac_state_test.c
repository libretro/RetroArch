/* raac's savestates: a decoder saved between access units and loaded
 * into a new one decodes on exactly as the saved one does.
 *
 * Each stream is decoded straight through once. Then one decoder takes
 * it again, saved before every access unit k; each state goes into a
 * new decoder that takes the next WINDOW units:
 *
 *   S1  the loaded decoder's samples are the straight decode's, exactly;
 *   S2  the saved decoder's are too, through the whole stream;
 *   S3  a loaded decoder saves the same bytes again;
 *   S4  a state is refused at another size or from a decoder opened
 *       another way, and without it the next unit decodes otherwise
 *       (so a state left unread would show).
 *
 * The streams cover mono, stereo with M/S and intensity, 5.1, PNS
 * (whose generator is part of the state), TNS, short windows, and a
 * 22.05 kHz stream, which raac decodes at the doubled rate through the
 * QMF banks and their histories.
 *
 * Needs ffmpeg in PATH; a missing tool is a failure, not a skip. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>

#include <formats/raac.h>

#define WINDOW    6
#define MAX_UNITS 1024

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

typedef struct
{
   uint8_t       *file;
   uint8_t        asc[2];
   int            n;
   const uint8_t *data[MAX_UNITS];
   size_t         size[MAX_UNITS];
} stream_t;

/* An ADTS file as raw access units and the AudioSpecificConfig its
 * headers stand for. */
static int stream_open(stream_t *s, const char *path)
{
   FILE *f = fopen(path, "rb");
   long len;
   size_t pos = 0;
   memset(s, 0, sizeof(*s));
   if (!f)
      return -1;
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (len <= 0 || !(s->file = (uint8_t*)malloc((size_t)len))
         || fread(s->file, 1, (size_t)len, f) != (size_t)len)
   {
      fclose(f);
      return -1;
   }
   fclose(f);
   while (pos + 7 <= (size_t)len && s->n < MAX_UNITS)
   {
      const uint8_t *h = s->file + pos;
      size_t hdr, fl;
      if (h[0] != 0xFF || (h[1] & 0xF6) != 0xF0)
         return -1;
      hdr = (h[1] & 1) ? 7 : 9;
      fl  = ((size_t)(h[3] & 3) << 11) | ((size_t)h[4] << 3) | (h[5] >> 5);
      if (fl < hdr || pos + fl > (size_t)len)
         return -1;
      if (!s->n)
      {
         unsigned ot = ((h[2] >> 6) & 3) + 1, sfi = (h[2] >> 2) & 15;
         unsigned cc = ((h[2] & 1) << 2) | (h[3] >> 6);
         s->asc[0] = (uint8_t)((ot << 3) | (sfi >> 1));
         s->asc[1] = (uint8_t)(((sfi & 1) << 7) | (cc << 3));
      }
      s->data[s->n] = h + hdr;
      s->size[s->n] = fl - hdr;
      s->n++;
      pos += fl;
   }
   return s->n > 1 ? 0 : -1;
}

/* Samples of every unit, straight through: units * frame * channels. */
static int16_t *g_ref;
static size_t   g_unit;   /* samples a unit */

static int decode(raac_t *a, const stream_t *s, int k, int16_t *out)
{
   return raac_decode_s16(a, s->data[k], s->size[k], out)
      == (int)raac_frame_len(a) ? 0 : -1;
}

static int same(const int16_t *pcm, int k)
{
   return !memcmp(pcm, g_ref + (size_t)k * g_unit, g_unit * sizeof(int16_t));
}

static void state_case(const char *name, const char *src, const char *enc)
{
   char path[512], label[160];
   stream_t s;
   raac_t *a = NULL, *b = NULL, *o = NULL;
   int16_t *pcm = NULL;
   uint8_t *st = NULL, *again = NULL;
   int k, i, loaded = 0, bad1 = 0, bad2 = 0, bad3 = 0, ok;
   size_t n = 0;
   snprintf(path, sizeof(path), "%s/%s.aac", dir, name);
   printf("%s (%s):\n", name, enc);
   if (run("ffmpeg -v error -y -f lavfi -i \"%s\" -c:a aac %s -f adts '%s'",
            src, enc, path) != 0 || stream_open(&s, path) != 0
         || !(a = raac_open(s.asc, 2)))
   {
      check("fixture", 0);
      goto done;
   }
   g_unit = (size_t)raac_frame_len(a) * raac_channels(a);
   g_ref  = (int16_t*)malloc(g_unit * sizeof(int16_t) * (size_t)s.n);
   pcm    = (int16_t*)malloc(g_unit * sizeof(int16_t));
   n      = raac_state_size(a);
   st     = (uint8_t*)malloc(n);
   again  = (uint8_t*)malloc(n);
   if (!g_ref || !pcm || !st || !again)
      goto done;
   printf("      %d units, %u channels at %u Hz, %u-byte states\n", s.n,
         raac_channels(a), raac_sample_rate(a), (unsigned)n);
   for (k = 0; k < s.n; k++)
      if (decode(a, &s, k, g_ref + (size_t)k * g_unit) != 0)
         break;
   check("straight decode", k == s.n);
   raac_close(a);
   if (k != s.n || !(a = raac_open(s.asc, 2)))
      goto done;

   for (k = 0; k < s.n; k++)
   {
      if (k && (b = raac_open(s.asc, 2)) != NULL)
      {
         raac_save_state(a, st);
         if (raac_load_state(b, st, n) != 0)
            bad1++;
         else
         {
            loaded++;
            raac_save_state(b, again);
            if (memcmp(again, st, n))
               bad3++;
            for (i = k; i < s.n && i < k + WINDOW; i++)
               if (decode(b, &s, i, pcm) != 0 || !same(pcm, i))
               {
                  bad1++;
                  break;
               }
         }
         raac_close(b);
         b = NULL;
      }
      if (decode(a, &s, k, pcm) != 0 || !same(pcm, k))
         bad2++;
   }
   snprintf(label, sizeof(label), "S1 loaded decoders continue exactly (%d states)", loaded);
   check(label, bad1 == 0 && loaded == s.n - 1);
   check("S2 the saved decoder continues exactly", bad2 == 0);
   check("S3 a loaded decoder saves the same bytes", bad3 == 0);

   /* S4: the state halfway, refused where it does not fit */
   raac_close(a);
   if (!(a = raac_open(s.asc, 2)))
      goto done;
   for (k = 0; k < s.n / 2; k++)
      decode(a, &s, k, pcm);
   raac_save_state(a, st);
   b = raac_open(s.asc, 2);
   ok = b && raac_load_state(b, st, n - 1) == -1
         && raac_load_state(b, st, n + 1) == -1;
   st[4] ^= 1;
   ok = ok && raac_load_state(b, st, n) == -1;
   st[4] ^= 1;
   check("S4 a state of another size or layout is refused", ok);
   {
      /* the same stream state under another configuration: mono and
       * stereo LC at 48 kHz are told apart from this one */
      static const uint8_t mono48[2]   = { 0x11, 0x88 };
      static const uint8_t stereo48[2] = { 0x11, 0x90 };
      o = raac_open(raac_channels(a) == 1 ? stereo48 : mono48, 2);
      check("S4 a state from a decoder opened otherwise is refused",
            o && raac_load_state(o, st, raac_state_size(o)) == -1);
   }
   ok = b && decode(b, &s, s.n / 2, pcm) == 0 && !same(pcm, s.n / 2);
   check("S4 without the state the next unit decodes otherwise", ok);
done:
   if (a) raac_close(a);
   if (b) raac_close(b);
   if (o) raac_close(o);
   free(pcm);
   free(st);
   free(again);
   free(g_ref);
   g_ref = NULL;
   free(s.file);
}

int main(void)
{
   snprintf(dir, sizeof(dir), "/tmp/raac_state_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 1;
   /* noise bursts against tones: transients for short windows, and
    * noise bands for PNS */
   state_case("mono_pns",
         "aevalsrc='0.3*sin(440*2*PI*t)+0.4*random(0)*lt(mod(t,0.5),0.05):s=44100:d=3'",
         "-ac 1 -b:a 64k -aac_pns 1 -aac_tns 1");
   state_case("stereo_ms_is",
         "aevalsrc='0.4*sin(330*2*PI*t)+0.2*random(0)*lt(mod(t,0.37),0.03)|0.35*sin(332*2*PI*t)+0.2*random(1)*lt(mod(t,0.41),0.02):s=48000:d=3'",
         "-ac 2 -b:a 96k -aac_ms 1 -aac_is 1 -aac_tns 1");
   state_case("surround51",
         "aevalsrc='sin(200*2*PI*t)*0.3|sin(300*2*PI*t)*0.3|random(0)*0.1|sin(50*2*PI*t)*0.3|sin(500*2*PI*t)*0.2|sin(600*2*PI*t)*0.2:s=48000:d=2'",
         "-ac 6 -b:a 192k");
   state_case("qmf_22k",
         "aevalsrc='0.4*sin(440*2*PI*t)+0.2*random(0)*lt(mod(t,0.3),0.04):s=22050:d=3'",
         "-ac 2 -b:a 48k -aac_pns 1");
   run("rm -rf '%s'", dir);
   printf("raac_state_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}

/* The ALSA driver's speaker order, against the null PCM.
 *
 * The frontend lays a frame out in ascending speaker-bit order; a card
 * takes its channels in the order its channel map gives, and a PCM
 * with no map in ALSA's own order (FL FR RL RR FC LFE SL SR). The
 * driver has to report the layout the card has and hand it the
 * frontend's channels in the card's order, by write() and through a
 * lent span alike. The map, the channel counts the card takes and the
 * write are scripted with -Wl,--wrap; everything else is the real
 * library. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>
#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"

extern audio_driver_t audio_alsa;
extern uint32_t test_requested_layout;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* the card */
static unsigned scr_max_channels = 8;
static int      scr_map_n;            /* 0: no map */
static int      scr_map[8];
static int      scr_first_short;      /* the first writei takes one frame */
static int      scr_fake_hw;          /* type HW, so the mmap path opens */

/* what reached it */
static unsigned char got[4096];
static size_t        got_bytes;
static unsigned      frame_bytes;

snd_pcm_chmap_t *__wrap_snd_pcm_get_chmap(snd_pcm_t *pcm)
{
   snd_pcm_chmap_t *m;
   int i;
   (void)pcm;
   if (!scr_map_n)
      return NULL;
   m = (snd_pcm_chmap_t*)malloc(sizeof(*m) + scr_map_n * sizeof(m->pos[0]));
   m->channels = scr_map_n;
   for (i = 0; i < scr_map_n; i++)
      m->pos[i] = scr_map[i];
   return m;
}

int __real_snd_pcm_hw_params_test_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *p, unsigned int val);
int __wrap_snd_pcm_hw_params_test_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *p, unsigned int val)
{
   if (val > scr_max_channels)
      return -EINVAL;
   return __real_snd_pcm_hw_params_test_channels(pcm, p, val);
}

snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t *pcm, const void *buf, snd_pcm_uframes_t size)
{
   (void)pcm;
   if (scr_first_short && size > 1)
   {
      scr_first_short = 0;
      size = 1;
   }
   if (got_bytes + size * frame_bytes <= sizeof(got))
   {
      memcpy(got + got_bytes, buf, size * frame_bytes);
      got_bytes += size * frame_bytes;
   }
   return (snd_pcm_sframes_t)size;
}

snd_pcm_type_t __real_snd_pcm_type(snd_pcm_t *pcm);
snd_pcm_type_t __wrap_snd_pcm_type(snd_pcm_t *pcm)
{
   if (scr_fake_hw)
      return SND_PCM_TYPE_HW;
   return __real_snd_pcm_type(pcm);
}

/* sample k of frame f, from the frontend: f * 10 + k + 1 */
static float sample_at(const unsigned char *p, unsigned idx, bool flt)
{
   if (flt)
   {
      float v;
      memcpy(&v, p + idx * 4, 4);
      return v;
   }
   {
      int16_t v;
      memcpy(&v, p + idx * 2, 2);
      return (float)v;
   }
}

static void fill(unsigned char *p, unsigned frames, unsigned ch, bool flt)
{
   unsigned f, k;
   for (f = 0; f < frames; f++)
      for (k = 0; k < ch; k++)
      {
         float v = (float)(f * 10 + k + 1);
         if (flt)
            memcpy(p + (f * ch + k) * 4, &v, 4);
         else
         {
            int16_t s = (int16_t)v;
            memcpy(p + (f * ch + k) * 2, &s, 2);
         }
      }
}

/* Three frames through write(), the first writei short; the card's
 * channel k of frame f must carry frontend slot expect[k] of that
 * frame. */
static void run(const char *what, uint32_t asked, unsigned expect_ch,
      uint32_t expect_layout, const unsigned *expect)
{
   unsigned char in[3 * 8 * 4];
   unsigned new_rate = 0, f, k;
   bool flt;
   void *h;

   printf("   %s\n", what);
   test_requested_layout = asked;
   got_bytes             = 0;
   scr_first_short       = 1;
   h = audio_alsa.init("null", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return;
   flt         = audio_alsa.use_float(h);
   CHECK(audio_alsa.layout(h) == expect_layout, "layout 0x%03x, 0x%03x expected",
         audio_alsa.layout(h), expect_layout);
   frame_bytes = expect_ch * (flt ? 4 : 2);
   fill(in, 3, expect_ch, flt);
   CHECK(audio_alsa.write(h, in, 3 * frame_bytes) == (ssize_t)(3 * frame_bytes),
         "three frames were not taken");
   CHECK(got_bytes == 3 * frame_bytes, "%u bytes reached the card, %u expected",
         (unsigned)got_bytes, 3 * frame_bytes);
   for (f = 0; f < 3 && got_bytes == 3 * frame_bytes; f++)
      for (k = 0; k < expect_ch; k++)
      {
         float v = sample_at(got, f * expect_ch + k, flt);
         CHECK(v == (float)(f * 10 + expect[k] + 1),
               "frame %u, card channel %u carries %g, slot %u expected", f, k, v, expect[k]);
      }
   audio_alsa.free(h);
}

/* The same through a lent span: the frontend writes the frame in its
 * order into the card's buffer, and the commit leaves it in the
 * card's. */
static void run_lend(void)
{
   static const unsigned alsa51[6] = { 0, 1, 4, 5, 2, 3 };
   unsigned new_rate = 0, k;
   void *region = NULL;
   size_t span;
   bool flt;
   void *h;

   printf("   a lent span on a card without a map: permuted in place\n");
   config_get_ptr()->bools.audio_threaded_pipeline = true;
   scr_fake_hw           = 1;
   scr_map_n             = 0;
   scr_max_channels      = 6;
   test_requested_layout = AUDIO_LAYOUT_5POINT1;
   h = audio_alsa.init("null", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      flt         = audio_alsa.use_float(h);
      frame_bytes = 6 * (flt ? 4 : 2);
      span = audio_alsa.write_begin(h, frame_bytes, &region);
      if (!span)
         printf("      [skip] the null PCM lends no span here\n");
      else
      {
         fill((unsigned char*)region, 1, 6, flt);
         CHECK(audio_alsa.write_end(h, frame_bytes) == (ssize_t)frame_bytes,
               "the commit did not take the frame");
         for (k = 0; k < 6; k++)
         {
            float v = sample_at((const unsigned char*)region, k, flt);
            CHECK(v == (float)(alsa51[k] + 1),
                  "card channel %u carries %g, slot %u expected", k, v, alsa51[k]);
         }
      }
      audio_alsa.free(h);
   }
   config_get_ptr()->bools.audio_threaded_pipeline = false;
   scr_fake_hw = 0;
}

int main(void)
{
   static const unsigned same[8]   = { 0, 1, 2, 3, 4, 5, 6, 7 };
   static const unsigned alsa51[6] = { 0, 1, 4, 5, 2, 3 };
   static const unsigned alsa71[8] = { 0, 1, 4, 5, 2, 3, 6, 7 };

   printf("alsa speaker order:\n");

   scr_map_n = 0; scr_max_channels = 2;
   run("a stereo card", AUDIO_LAYOUT_5POINT1, 2, AUDIO_LAYOUT_STEREO, same);

   scr_map_n = 0; scr_max_channels = 6;
   run("a 5.1 card without a map, 7.1 asked for: 5.1 in ALSA's order",
         AUDIO_LAYOUT_7POINT1, 6, AUDIO_LAYOUT_5POINT1, alsa51);

   scr_map_n = 0; scr_max_channels = 8;
   run("a 7.1 card without a map: ALSA's order",
         AUDIO_LAYOUT_7POINT1, 8, AUDIO_LAYOUT_7POINT1, alsa71);

   scr_max_channels = 8; scr_map_n = 6;
   scr_map[0] = SND_CHMAP_FL; scr_map[1] = SND_CHMAP_FR; scr_map[2] = SND_CHMAP_RL;
   scr_map[3] = SND_CHMAP_RR; scr_map[4] = SND_CHMAP_FC; scr_map[5] = SND_CHMAP_LFE;
   run("an HDA map, FL FR RL RR FC LFE", AUDIO_LAYOUT_5POINT1, 6, AUDIO_LAYOUT_5POINT1, alsa51);

   scr_map[2] = SND_CHMAP_FC; scr_map[3] = SND_CHMAP_LFE;
   scr_map[4] = SND_CHMAP_SL; scr_map[5] = SND_CHMAP_SR;
   run("a USB map, FL FR FC LFE SL SR: 5.1 (side), as it is",
         AUDIO_LAYOUT_5POINT1, 6, AUDIO_LAYOUT_5POINT1_SURROUND, same);

   scr_max_channels = 4; scr_map_n = 0;
   run("a quad card, 7.1 asked for", AUDIO_LAYOUT_7POINT1, 4, AUDIO_LAYOUT_QUAD, same);

   run_lend();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("alsa speaker order: the card gets its own order, by write and by lend\n");
   return 0;
}

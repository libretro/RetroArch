/* audio/drivers/rwebaudio.c's write, against a stand-in for its
 * JavaScript half: the frontend's interleaved frames of the layout the
 * page opened go out as one plane per channel, every byte count is in
 * frames of that layout, and a page whose output is stereo keeps the
 * stereo it always had. The JavaScript half itself is run under Node
 * by rwebaudio_js_test.js. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../audio/drivers/rwebaudio.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

static uint32_t want_layout = AUDIO_LAYOUT_STEREO;
uint32_t audio_driver_requested_layout(void) { return want_layout; }

/* the page: how wide its output goes, and what it was handed */
static unsigned page_max = 2, page_channels;
static float    queued[8][480];
static size_t   queued_frames;

unsigned RWebAudioSampleRate(void) { return 48000; }
unsigned RWebAudioInit(unsigned latency, unsigned channels)
{
   (void)latency;
   while (channels > 2 && page_max < channels)
      channels = channels > 6 ? 6 : channels > 4 ? 4 : 2;
   page_channels = channels > 2 ? channels : 2;
   return page_channels;
}
ssize_t RWebAudioQueueBuffer(size_t num_frames, float *planes, size_t plane_frames)
{
   unsigned c;
   for (c = 0; c < page_channels; c++)
      memcpy(queued[c], planes + c * plane_frames, num_frames * sizeof(float));
   queued_frames = num_frames;
   return (ssize_t)num_frames;
}
bool RWebAudioStop(void) { return true; }
bool RWebAudioStart(void) { return true; }
void RWebAudioSetNonblockState(bool state) { (void)state; }
void RWebAudioFree(void) { }
size_t RWebAudioWriteAvailFrames(void) { return 4800; }
size_t RWebAudioBufferSizeFrames(void) { return 3072; }
void RWebAudioRecalibrateTime(void) { }
bool RWebAudioResumeCtx(void) { return true; }

static void run(const char *what, uint32_t asked, unsigned max,
      unsigned expect_ch, uint32_t expect_layout)
{
   static float in[480 * 8];
   unsigned new_rate = 0, f, c;
   void *h;
   printf("   %s\n", what);
   want_layout   = asked;
   page_max      = max;
   queued_frames = 0;
   h = audio_rwebaudio.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return;
   CHECK(audio_rwebaudio.layout(h) == expect_layout, "layout 0x%03x, 0x%03x expected",
         audio_rwebaudio.layout(h), expect_layout);
   CHECK(audio_rwebaudio.buffer_size(h) == 3072 * expect_ch * sizeof(float),
         "buffer_size %u is not %u channels of 3072 frames",
         (unsigned)audio_rwebaudio.buffer_size(h), expect_ch);
   for (f = 0; f < 480; f++)
      for (c = 0; c < expect_ch; c++)
         in[f * expect_ch + c] = (float)(c * 1000 + f);
   CHECK(audio_rwebaudio.write(h, in, 480 * expect_ch * sizeof(float))
         == (ssize_t)(480 * expect_ch * sizeof(float)), "the write came back short");
   CHECK(queued_frames == 480, "%u frames queued, a 10 ms block expected", (unsigned)queued_frames);
   for (c = 0; c < expect_ch && queued_frames == 480; c++)
      for (f = 0; f < 480; f += 97)
         CHECK(queued[c][f] == (float)(c * 1000 + f), "plane %u frame %u is %g", c, f, queued[c][f]);
   audio_rwebaudio.free(h);
}

int main(void)
{
   printf("rwebaudio:\n");
   run("a stereo page, stereo asked for", AUDIO_LAYOUT_STEREO, 2, 2, AUDIO_LAYOUT_STEREO);
   run("a stereo page, 5.1 asked for: stereo", AUDIO_LAYOUT_5POINT1, 2, 2, AUDIO_LAYOUT_STEREO);
   run("a 5.1 page, 7.1 asked for: 5.1", AUDIO_LAYOUT_7POINT1, 6, 6, AUDIO_LAYOUT_5POINT1);
   run("a 7.1 page, 7.1 asked for", AUDIO_LAYOUT_7POINT1, 8, 8, AUDIO_LAYOUT_7POINT1);
   run("a 7.1 page, quad asked for", AUDIO_LAYOUT_QUAD, 8, 4, AUDIO_LAYOUT_QUAD);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rwebaudio: one plane per channel of the layout the page took\n");
   return 0;
}

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
/* what the page reports free, and how often it was asked */
static size_t   page_avail = 4800;
static unsigned avail_asks, resumes;
size_t RWebAudioWriteAvailFrames(void) { avail_asks++; return page_avail; }
size_t RWebAudioBufferSizeFrames(void) { return 3072; }
void RWebAudioRecalibrateTime(void) { }
bool RWebAudioResumeCtx(void) { resumes++; return true; }

/* The main loop's block, as the published player fakes it. */
static unsigned blocks_entered, blocks_left;
void platform_emscripten_enter_fake_block(int ms) { (void)ms; blocks_entered++; }
void platform_emscripten_exit_fake_block(void) { blocks_left++; }

/* A blocking write to a page with no room, on the published player's
 * build, where nothing on the page's thread can wait: the write asks
 * the main loop to block and returns, and the block goes back to the
 * browser until there is room - neither spins asking */
static void run_full(void)
{
   static float in[480 * 2];
   unsigned new_rate = 0;
   void *h;
   printf("   a blocking write to a full page, the main loop's block\n");
   want_layout = AUDIO_LAYOUT_STEREO;
   page_max    = 2;
   h = audio_rwebaudio.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return;
   audio_rwebaudio.set_nonblock_state(h, false);
   page_avail     = 0;
   avail_asks     = resumes = 0;
   blocks_entered = blocks_left = 0;
   audio_rwebaudio.write(h, in, sizeof(in));
   CHECK(blocks_entered == 1, "the write asked for the block %u times", blocks_entered);
   CHECK(avail_asks <= 4, "the page was asked for room %u times", avail_asks);

   /* No room yet: back to the browser, after one look. */
   avail_asks = 0;
   CHECK(rwebaudio_external_block(), "the block ended with no room");
   CHECK(avail_asks <= 2, "the block asked for room %u times", avail_asks);
   CHECK(blocks_left == 0, "the block was left with no room");

   /* Room: the block is left, once. */
   page_avail = 4800;
   rwebaudio_external_block();
   CHECK(blocks_left == 1, "the block was left %u times", blocks_left);
   CHECK(!rwebaudio_external_block(), "a block nobody asked for went on");
   audio_rwebaudio.free(h);
}

/* The JavaScript half's wait for room, as the asyncify builds await
 * it: each call is one of the context's clock events - a source ended,
 * which frees room only once enough have - or, for a context that will
 * not run, its watchdog. */
extern audio_driver_t audio_rwebaudio_asyncify;
static unsigned waits, wakes_before_room;
static bool     context_stopped;
int RWebAudioWaitRoom(unsigned ms)
{
   (void)ms;
   waits++;
   if (context_stopped)
      return 0;
   if (waits > wakes_before_room)
      page_avail = 4800;
   return 1;
}

static void run_asyncify(void)
{
   static float in[480 * 2];
   unsigned new_rate = 0;
   void *h;
   printf("   a blocking write to a full page, awaiting the context's clock\n");
   want_layout = AUDIO_LAYOUT_STEREO;
   page_max    = 2;
   h = audio_rwebaudio_asyncify.init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return;
   audio_rwebaudio_asyncify.set_nonblock_state(h, false);

   /* Sources end one by one, and the twentieth frees room: the write
    * waits through all of them, each a wake rather than a watchdog. */
   page_avail        = 0;
   waits             = 0;
   wakes_before_room = 20;
   context_stopped   = false;
   blocks_entered    = 0;
   audio_rwebaudio_asyncify.write(h, in, sizeof(in));
   CHECK(waits == 21, "the write waited %u times for 21 clock events", waits);
   CHECK(page_avail == 4800, "the write gave up before the room came");
   CHECK(blocks_entered == 0, "the asyncify write asked for the main loop's block");

   /* A context that will not run: watchdogs only, and the write gives
    * up after its laps rather than holding the page. */
   page_avail      = 0;
   waits           = 0;
   context_stopped = true;
   audio_rwebaudio_asyncify.write(h, in, sizeof(in));
   CHECK(waits > 1 && waits <= 16, "a stopped context was waited on %u times", waits);

   page_avail      = 4800;
   context_stopped = false;
   audio_rwebaudio_asyncify.free(h);
}

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
   run_full();
   run_asyncify();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rwebaudio: one plane per channel of the layout the page took\n");
   return 0;
}

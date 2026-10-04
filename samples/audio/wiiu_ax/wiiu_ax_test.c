/* The Wii U AX driver's frame accounting, with the frame callback and
 * the writer on different threads.
 *
 * AX calls the driver's frame callback every 3 ms from its own thread;
 * it takes a frame's worth off the count of frames buffered while the
 * writer, on the audio thread, adds what it wrote. The count is one
 * atomic word and neither side takes a lock, so the two must still
 * add up: every frame the driver accepted is either still buffered or
 * was counted as played. A lost update on either side breaks that sum.
 *
 * The driver is the real audio/drivers/wiiu_audio.c, built against the
 * tree's own Wii U headers; the AX and OS functions behind them are
 * stand-ins here, with the voice's running state the only thing they
 * keep. Built under ThreadSanitizer as well: a field the two threads
 * share without ordering is a reported race.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>

#include "../../../audio/drivers/wiiu_audio.c"

#define CALLBACKS 200000
#define WRITES    60000

/* --- the platform, as far as the driver goes ------------------------- */

static retro_atomic_int_t mock_state;
static OSSystemInfo       mock_sysinfo;

OSSystemInfo *OSGetSystemInfo()
{
   mock_sysinfo.clockSpeed = 248625000;
   return &mock_sysinfo;
}

void AXInitWithParams(AXInitParams *params) { (void)params; }
void AXQuit() { }
void AXRegisterFrameCallback(void *cb) { (void)cb; }

void AXAcquireMultiVoice(u32 prio, void *cb, u32 cbarg, void *setup,
      AXMVoice **mvoice)
{
   (void)prio; (void)cb; (void)cbarg; (void)setup;
   *mvoice = (AXMVoice*)calloc(1, sizeof(AXMVoice) + 2 * sizeof(AXVoice*));
   if (*mvoice)
      (*mvoice)->channels = 2;
}

void AXFreeMultiVoice(AXMVoice *mvoice) { free(mvoice); }

void AXSetMultiVoiceState(AXMVoice *mvoice, AXVoiceState state)
{
   (void)mvoice;
   retro_atomic_store_release_int(&mock_state, (int)state);
}

bool AXIsMultiVoiceRunning(AXMVoice *mvoice)
{
   (void)mvoice;
   return retro_atomic_load_acquire_int(&mock_state)
      == (int)AX_VOICE_STATE_PLAYING;
}

void AXSetMultiVoiceDeviceMix(AXMVoice *mvoice, AXDeviceType type, u32 id,
      u32 bus, u16 vol, s16 delta)
{ (void)mvoice; (void)type; (void)id; (void)bus; (void)vol; (void)delta; }
void AXSetMultiVoiceOffsets(AXMVoice *mvoice, AXVoiceOffsets *offsets)
{ (void)mvoice; (void)offsets; }
void AXSetMultiVoiceCurrentOffset(AXMVoice *mvoice, uint32_t offset)
{ (void)mvoice; (void)offset; }
void AXSetMultiVoiceVe(AXMVoice *mvoice, AXVoiceVeData *veData)
{ (void)mvoice; (void)veData; }
void AXSetMultiVoiceSrcType(AXMVoice *mvoice, AXVoiceSrcType type)
{ (void)mvoice; (void)type; }
void AXSetMultiVoiceSrcRatio(AXMVoice *mvoice, float ratio)
{ (void)mvoice; (void)ratio; }

void OSInitEvent(OSEvent *event, BOOL value, OSEventMode mode)
{ (void)event; (void)value; (void)mode; }
void OSSignalEvent(OSEvent *event) { (void)event; }
BOOL OSWaitEventWithTimeout(OSEvent *event, OSTime timeout)
{
   (void)event; (void)timeout;
   sthread_yield();
   return TRUE;
}

void DCFlushRange(void *addr, uint32_t size) { (void)addr; (void)size; }
void DCStoreRange(void *addr, uint32_t size) { (void)addr; (void)size; }
void DCStoreRangeNoSync(void *addr, uint32_t size) { (void)addr; (void)size; }

void *MEM1_alloc(unsigned int size, unsigned int align)
{
   (void)align;
   return calloc(1, size);
}
void MEM1_free(void *ptr) { free(ptr); }

/* --- the test --------------------------------------------------------- */

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static retro_atomic_int_t ax_go;

/* AX's thread: a frame callback, over and over. */
static void ax_thread(void *unused)
{
   unsigned i;
   (void)unused;
   while (!retro_atomic_load_acquire_int(&ax_go))
      sthread_yield();
   for (i = 0; i < CALLBACKS; i++)
   {
      wiiu_ax_callback();
      if (!(i & 15))
         sthread_yield();
   }
}

int main(void)
{
   static int16_t block[AX_AUDIO_COUNT * 2];
   unsigned    rate    = 0;
   unsigned    i, seed = 1;
   size_t      taken   = 0;   /* frames the driver accepted */
   int         out_of_range = 0;
   sthread_t  *thread;
   ax_audio_t *ax = (ax_audio_t*)audio_ax.init(NULL, 48000, 64, &rate);

   if (!ax)
   {
      printf("FAIL: the driver would not start\n");
      return 1;
   }
   audio_ax.set_nonblock_state(ax, true);

   /* One thread: what a frame costs and when the voice parks. */
   taken += (size_t)audio_ax.write(ax, block, 2000 * 4) / 4;
   check("a write is counted, and starts the voice",
            taken == 2000 && ax_audio_written(ax) == 2000
         && audio_ax.alive(ax));
   wiiu_ax_callback();
   check("a frame callback takes a frame's worth and counts it played",
            ax_audio_written(ax) == 2000 - AX_AUDIO_SAMPLE_COUNT
         && audio_ax.frames_consumed(ax) == AX_AUDIO_SAMPLE_COUNT);
   for (i = 0; i < 64 && audio_ax.alive(ax); i++)
      wiiu_ax_callback();
   check("the voice parks once, short of audio",
            !audio_ax.alive(ax) && audio_ax.underruns(ax) == 1
         && ax_audio_written(ax) < AX_AUDIO_SAMPLE_MIN);
   wiiu_ax_callback();
   check("a parked voice plays nothing",
         taken == ax_audio_written(ax) + audio_ax.frames_consumed(ax));

   /* Two threads: the callback against the writer. */
   if (!(thread = sthread_create(ax_thread, NULL)))
   {
      printf("FAIL: no thread\n");
      return 1;
   }
   retro_atomic_store_release_int(&ax_go, 1);
   for (i = 0; i < WRITES; i++)
   {
      uint32_t written;
      size_t   frames;
      seed    = seed * 1103515245u + 12345u;
      frames  = 16 + ((seed >> 16) % 700);
      taken  += (size_t)audio_ax.write(ax, block, frames * 4) / 4;
      written = ax_audio_written(ax);
      if (written > AX_AUDIO_MAX_FREE)
         out_of_range++;
      if (!(i & 7))
         sthread_yield();
   }
   sthread_join(thread);

   check("the count of buffered frames stayed inside the ring",
         !out_of_range);
   check("every frame accepted is buffered or was counted played",
         taken == ax_audio_written(ax) + audio_ax.frames_consumed(ax));
   printf("  %u frames written, %u played, %u underruns\n",
         (unsigned)taken, (unsigned)audio_ax.frames_consumed(ax),
         (unsigned)audio_ax.underruns(ax));

   audio_ax.free(ax);
   if (fails)
   {
      printf("wiiu_ax_test: FAIL (%d)\n", fails);
      return 1;
   }
   printf("wiiu_ax_test: PASS\n");
   return 0;
}

/* The renderer behind the stand-in <switch.h>. A renderer thread plays
 * the voice's oldest queued wave buffer once a frame by advancing a
 * played count; the buffer's state becomes Done only when the driver
 * next calls audrvUpdate(), as libnx reports a finished buffer. The
 * wave buffer list and states are plain memory, touched only by the
 * renderer calls: two threads in them at once is a data race TSan
 * reports, and a call from a thread other than the owner is counted
 * either way. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#include "switch.h"
#include "audren_mock.h"

static AudioDriverWaveBuf *queue_head;
static size_t              done_bytes;
static unsigned            updates;
static unsigned            consumed;       /* buffers marked Done */
static _Atomic unsigned    played;         /* buffers the renderer played */
static _Atomic int         playing;
static _Atomic unsigned    foreign;
static _Atomic size_t      played_bytes;
static pthread_t           owner;
static _Atomic int         owner_set;
static pthread_t           renderer;
static _Atomic int         renderer_quit;
static unsigned            frame_us_cfg;
static pthread_mutex_t     ev_mtx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t      ev_cond = PTHREAD_COND_INITIALIZER;
static unsigned long       ev_seq;
static Event               frame_event;

static void check_owner(void)
{
   if (atomic_load(&owner_set) && !pthread_equal(pthread_self(), owner))
      atomic_fetch_add(&foreign, 1);
}

void audren_mock_set_owner(pthread_t t) { owner = t; atomic_store(&owner_set, 1); }
unsigned audren_mock_foreign_calls(void) { return atomic_load(&foreign); }
size_t audren_mock_played_bytes(void) { return atomic_load(&played_bytes); }
unsigned audren_mock_updates(void) { return updates; }

/* Buffers queued and not yet played, read by the renderer thread: the
 * queue length is published as a count so the renderer never walks the
 * driver's list. */
static _Atomic unsigned queued_count;

static void *renderer_main(void *arg)
{
   (void)arg;
   while (!atomic_load(&renderer_quit))
   {
      struct timespec ts = { 0, (long)frame_us_cfg * 1000 };
      nanosleep(&ts, NULL);
      if (atomic_load(&playing) && atomic_load(&queued_count) > atomic_load(&played))
         atomic_fetch_add(&played, 1);
      pthread_mutex_lock(&ev_mtx);
      ev_seq++;
      pthread_cond_broadcast(&ev_cond);
      pthread_mutex_unlock(&ev_mtx);
   }
   return NULL;
}

void audren_mock_start(unsigned frame_us)
{
   frame_us_cfg = frame_us;
   atomic_store(&renderer_quit, 0);
   pthread_create(&renderer, NULL, renderer_main, NULL);
}

void audren_mock_stop(void)
{
   atomic_store(&renderer_quit, 1);
   pthread_join(renderer, NULL);
}

Result audrenInitialize(const AudioRendererConfig *c) { (void)c; return 0; }
void   audrenExit(void) { }
Result audrenStartAudioRenderer(void) { return 0; }
Event *audrenGetFrameEvent(void) { return &frame_event; }

Result eventWait(Event *e, u64 timeout_ns)
{
   struct timespec ts;
   unsigned long seq;
   int rc = 0;
   (void)e;
   clock_gettime(CLOCK_REALTIME, &ts);
   ts.tv_sec  += (time_t)(timeout_ns / 1000000000ULL);
   ts.tv_nsec += (long)(timeout_ns % 1000000000ULL);
   if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
   pthread_mutex_lock(&ev_mtx);
   seq = ev_seq;
   while (seq == ev_seq && rc == 0)
      rc = pthread_cond_timedwait(&ev_cond, &ev_mtx, &ts);
   rc = (seq == ev_seq) ? 1 : 0;
   pthread_mutex_unlock(&ev_mtx);
   return (Result)rc;
}

Result audrvCreate(AudioDriver *d, const AudioRendererConfig *c, int n)
{
   (void)c;
   memset(d, 0, sizeof(*d));
   d->channels  = (unsigned)n;
   queue_head   = NULL;
   done_bytes   = 0;
   updates      = 0;
   consumed     = 0;
   atomic_store(&played, 0);
   atomic_store(&queued_count, 0);
   atomic_store(&played_bytes, 0);
   atomic_store(&foreign, 0);
   atomic_store(&owner_set, 0);
   return 0;
}
void audrvClose(AudioDriver *d) { (void)d; }

Result audrvUpdate(AudioDriver *d)
{
   unsigned now = atomic_load(&played);
   (void)d;
   check_owner();
   updates++;
   while (consumed < now && queue_head)
   {
      AudioDriverWaveBuf *b = queue_head;
      size_t bytes = (size_t)(b->end_sample_offset - b->start_sample_offset)
            * d->channels * sizeof(int16_t);
      b->state    = AudioDriverWaveBufState_Done;
      queue_head  = b->next;
      b->next     = NULL;
      consumed++;
      atomic_fetch_add(&played_bytes, bytes);
   }
   return 0;
}

int  audrvMemPoolAdd(AudioDriver *d, void *b, size_t s) { (void)d; (void)b; (void)s; return 0; }
bool audrvMemPoolAttach(AudioDriver *d, int id) { (void)d; (void)id; return true; }
int  audrvDeviceSinkAdd(AudioDriver *d, const char *n, int c, const u8 *ids) { (void)d; (void)n; (void)c; (void)ids; return 0; }
bool audrvVoiceInit(AudioDriver *d, int id, int c, PcmFormat f, u32 r) { (void)d; (void)id; (void)c; (void)f; (void)r; return true; }
void audrvVoiceSetDestinationMix(AudioDriver *d, int id, int m) { (void)d; (void)id; (void)m; }
void audrvVoiceSetMixFactor(AudioDriver *d, int id, float f, int s, int t) { (void)d; (void)id; (void)f; (void)s; (void)t; }

bool audrvVoiceAddWaveBuf(AudioDriver *d, int id, AudioDriverWaveBuf *w)
{
   AudioDriverWaveBuf **p = &queue_head;
   (void)d; (void)id;
   check_owner();
   w->state = AudioDriverWaveBufState_Queued;
   w->next  = NULL;
   while (*p)
      p = &(*p)->next;
   *p = w;
   atomic_fetch_add(&queued_count, 1);
   return true;
}

void audrvVoiceStart(AudioDriver *d, int id) { (void)id; check_owner(); d->voice_playing = 1; atomic_store(&playing, 1); }
void audrvVoiceStop(AudioDriver *d, int id)  { (void)id; check_owner(); d->voice_playing = 0; atomic_store(&playing, 0); }
bool audrvVoiceIsPlaying(AudioDriver *d, int id) { (void)id; check_owner(); return d->voice_playing != 0; }

void armDCacheFlush(void *addr, size_t size) { (void)addr; (void)size; }

/* The scripted player behind the mock OpenSL headers: a buffer queue
 * a thread drains at the rate the format asks for, which is what the
 * driver's accounting and its waits are written against. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"

static const int iid_engine = 1, iid_play = 2, iid_bufq = 3;
const SLInterfaceID SL_IID_ENGINE = &iid_engine;
const SLInterfaceID SL_IID_PLAY   = &iid_play;
const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = &iid_bufq;

#define MOCK_QUEUE_MAX 64

static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static struct
{
   unsigned bytes[MOCK_QUEUE_MAX];
   unsigned head, count;
} q;

static void (*q_cb)(SLAndroidSimpleBufferQueueItf, void*);
static void  *q_cb_ctx;
static SLAndroidSimpleBufferQueueItf q_itf;

static int      mock_float_ok = 1, mock_frozen, mock_playing, mock_objects, mock_is_float;
static unsigned mock_num_buffers, mock_buffer_bytes, mock_rate_milli, mock_queue_limit;
static unsigned mock_enq_fail;
static size_t   mock_consumed;
static pthread_t pump;
static int      pump_run;

void opensl_mock_set_float_supported(int on) { mock_float_ok = on; }
void opensl_mock_set_queue_limit(unsigned n) { mock_queue_limit = n; }
void opensl_mock_freeze(int on)
{
   pthread_mutex_lock(&q_lock);
   mock_frozen = on;
   pthread_mutex_unlock(&q_lock);
}
unsigned opensl_mock_num_buffers(void)       { return mock_num_buffers; }
unsigned opensl_mock_buffer_bytes(void)      { return mock_buffer_bytes; }
unsigned opensl_mock_rate_milli(void)        { return mock_rate_milli; }
int      opensl_mock_is_float(void)          { return mock_is_float; }
unsigned opensl_mock_enqueue_failures(void)  { return mock_enq_fail; }
int      opensl_mock_playing(void)           { return mock_playing; }
int      opensl_mock_objects(void)           { return mock_objects; }
size_t   opensl_mock_consumed(void)
{
   size_t n;
   pthread_mutex_lock(&q_lock);
   n = mock_consumed;
   pthread_mutex_unlock(&q_lock);
   return n;
}

/* The device: takes a block off the queue at the rate the format
 * says, and calls the driver's callback for it, as OpenSL does. */
static void *pump_thread(void *arg)
{
   (void)arg;
   while (__atomic_load_n(&pump_run, __ATOMIC_ACQUIRE))
   {
      unsigned bytes = 0, is_float = 0, rate_milli = 0;
      pthread_mutex_lock(&q_lock);
      is_float   = (unsigned)mock_is_float;
      rate_milli = mock_rate_milli;
      if (mock_playing && !mock_frozen && q.count)
      {
         bytes    = q.bytes[q.head];
         q.head   = (q.head + 1) % MOCK_QUEUE_MAX;
         q.count--;
         mock_consumed++;
      }
      pthread_mutex_unlock(&q_lock);
      if (bytes)
      {
         /* the time that block would take to play */
         unsigned frames = bytes / (is_float ? 8 : 4);
         unsigned usec   = rate_milli
               ? (unsigned)((uint64_t)frames * 1000000u / (rate_milli / 1000)) : 1000;
         if (usec > 100000) usec = 100000;
         usleep(usec);
         if (q_cb)
            q_cb(q_itf, q_cb_ctx);
      }
      else
         usleep(500);
   }
   return NULL;
}

void opensl_mock_reset(void)
{
   if (__atomic_load_n(&pump_run, __ATOMIC_ACQUIRE))
   {
      __atomic_store_n(&pump_run, 0, __ATOMIC_RELEASE);
      pthread_join(pump, NULL);
   }
   pthread_mutex_lock(&q_lock);
   memset(&q, 0, sizeof(q));
   q_cb = NULL; q_cb_ctx = NULL;
   mock_float_ok = 1; mock_frozen = 0; mock_playing = 0; mock_objects = 0;
   mock_is_float = 0; mock_num_buffers = mock_buffer_bytes = mock_rate_milli = 0;
   mock_queue_limit = 0; mock_enq_fail = 0; mock_consumed = 0;
   pthread_mutex_unlock(&q_lock);
   __atomic_store_n(&pump_run, 1, __ATOMIC_RELEASE);
   pthread_create(&pump, NULL, pump_thread, NULL);
}

/* ---- the buffer queue ------------------------------------------- */

static SLresult bq_enqueue(SLAndroidSimpleBufferQueueItf self, const void *buf, SLuint32 size)
{
   unsigned limit = mock_queue_limit ? mock_queue_limit : mock_num_buffers;
   (void)self; (void)buf;
   pthread_mutex_lock(&q_lock);
   if (q.count >= limit || q.count >= MOCK_QUEUE_MAX)
   {
      mock_enq_fail++;
      pthread_mutex_unlock(&q_lock);
      return SL_RESULT_PARAMETER_INVALID;
   }
   q.bytes[(q.head + q.count) % MOCK_QUEUE_MAX] = size;
   q.count++;
   mock_buffer_bytes = size;
   pthread_mutex_unlock(&q_lock);
   return SL_RESULT_SUCCESS;
}
static SLresult bq_clear(SLAndroidSimpleBufferQueueItf self)
{
   (void)self;
   pthread_mutex_lock(&q_lock);
   q.head = q.count = 0;
   pthread_mutex_unlock(&q_lock);
   return SL_RESULT_SUCCESS;
}
static SLresult bq_state(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *st)
{
   (void)self;
   pthread_mutex_lock(&q_lock);
   st->count = q.count; st->index = q.head;
   pthread_mutex_unlock(&q_lock);
   return SL_RESULT_SUCCESS;
}
static SLresult bq_register(SLAndroidSimpleBufferQueueItf self,
      void (*cb)(SLAndroidSimpleBufferQueueItf, void*), void *ctx)
{
   q_itf = self; q_cb = cb; q_cb_ctx = ctx;
   return SL_RESULT_SUCCESS;
}
static const struct SLAndroidSimpleBufferQueueItf_ bq_vt =
{ bq_enqueue, bq_clear, bq_state, bq_register };
static const struct SLAndroidSimpleBufferQueueItf_ *bq_obj = &bq_vt;

/* ---- play ------------------------------------------------------- */

static SLresult play_set_state(SLPlayItf self, SLuint32 state)
{
   (void)self;
   pthread_mutex_lock(&q_lock);
   mock_playing = (state == SL_PLAYSTATE_PLAYING);
   pthread_mutex_unlock(&q_lock);
   return SL_RESULT_SUCCESS;
}
static const struct SLPlayItf_ play_vt = { play_set_state };
static const struct SLPlayItf_ *play_obj = &play_vt;

/* ---- objects ---------------------------------------------------- */

static SLresult obj_realize(SLObjectItf self, SLboolean async)
{ (void)self; (void)async; return SL_RESULT_SUCCESS; }
static const struct SLObjectItf_ *player_obj_storage;

static void obj_destroy(SLObjectItf self)
{
   /* Destroying the player stops its callbacks before it returns -
    * OpenSL says so, and a driver that frees its handle after
    * destroying the player is right to. The pump kept calling back
    * into freed memory, which is the mock being unfaithful rather
    * than the driver being wrong. */
   if (self == &player_obj_storage)
   {
      pthread_mutex_lock(&q_lock);
      q_cb        = NULL;
      q_cb_ctx    = NULL;
      q.head      = q.count = 0;
      mock_playing = 0;
      pthread_mutex_unlock(&q_lock);
   }
   mock_objects--;
}
static SLresult obj_get_iface(SLObjectItf self, SLInterfaceID iid, void *out);

static const struct SLObjectItf_ obj_vt = { obj_realize, obj_get_iface, obj_destroy };
static const struct SLObjectItf_ *engine_obj = &obj_vt;
static const struct SLObjectItf_ *mix_obj    = &obj_vt;

static SLresult eng_create_mix(SLEngineItf self, SLObjectItf *mix,
      SLuint32 n, const SLInterfaceID *ids, const SLboolean *req)
{
   (void)self; (void)n; (void)ids; (void)req;
   mock_objects++;
   *mix = &mix_obj;
   return SL_RESULT_SUCCESS;
}

static SLresult eng_create_player(SLEngineItf self, SLObjectItf *player,
      SLDataSource *src, SLDataSink *sink, SLuint32 n,
      const SLInterfaceID *ids, const SLboolean *req)
{
   SLuint32 type;
   (void)self; (void)sink; (void)n; (void)ids; (void)req;
   if (!src || !src->pLocator || !src->pFormat)
      return SL_RESULT_PARAMETER_INVALID;
   pthread_mutex_lock(&q_lock);
   mock_num_buffers = ((SLDataLocator_AndroidSimpleBufferQueue*)src->pLocator)->numBuffers;
   type = *(SLuint32*)src->pFormat;
   if (type == SL_ANDROID_DATAFORMAT_PCM_EX)
   {
      SLAndroidDataFormat_PCM_EX *f = (SLAndroidDataFormat_PCM_EX*)src->pFormat;
      if (!mock_float_ok)
      {
         pthread_mutex_unlock(&q_lock);
         return SL_RESULT_PARAMETER_INVALID;
      }
      mock_is_float   = 1;
      mock_rate_milli = f->sampleRate;
   }
   else
   {
      SLDataFormat_PCM *f = (SLDataFormat_PCM*)src->pFormat;
      mock_is_float   = 0;
      mock_rate_milli = f->samplesPerSec;
   }
   mock_objects++;
   pthread_mutex_unlock(&q_lock);
   player_obj_storage = &obj_vt;
   *player = &player_obj_storage;
   return SL_RESULT_SUCCESS;
}

static const struct SLEngineItf_ eng_vt = { eng_create_mix, eng_create_player };
static const struct SLEngineItf_ *eng_obj = &eng_vt;

static SLresult obj_get_iface(SLObjectItf self, SLInterfaceID iid, void *out)
{
   (void)self;
   if (iid == SL_IID_ENGINE)
      *(SLEngineItf*)out = &eng_obj;
   else if (iid == SL_IID_PLAY)
      *(SLPlayItf*)out = &play_obj;
   else if (iid == SL_IID_ANDROIDSIMPLEBUFFERQUEUE)
      *(SLAndroidSimpleBufferQueueItf*)out = &bq_obj;
   else
      return SL_RESULT_PARAMETER_INVALID;
   return SL_RESULT_SUCCESS;
}

SLresult slCreateEngine(SLObjectItf *engine, SLuint32 n, const void *opts,
      SLuint32 nids, const SLInterfaceID *ids, const SLboolean *req)
{
   (void)n; (void)opts; (void)nids; (void)ids; (void)req;
   mock_objects++;
   *engine = &engine_obj;
   return SL_RESULT_SUCCESS;
}

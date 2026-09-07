/* The fake device behind fake_wasapi.h. Exclusive, event-driven: the
 * endpoint buffer is one period; each period the device takes the
 * buffer that was released for it, or counts the period unanswered,
 * then raises the event for the next. Shared: a padding that drains
 * a period's worth per period and fills by ReleaseBuffer. Runs on a
 * real-time clock, so the harness's timing is the driver's. */

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "fake_wasapi.h"

const GUID IID_IAudioClient        = { 1, 0, 0, {0} };
const GUID IID_IAudioRenderClient  = { 2, 0, 0, {0} };
const GUID IID_IAudioCaptureClient = { 3, 0, 0, {0} };
const GUID mmdevice_IID_IAudioClient3 = { 4, 0, 0, {0} };
const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 3, 0, 16, {0x80,0,0,0xaa,0,0x38,0x9b,0x71} };
const GUID KSDATAFORMAT_SUBTYPE_PCM        = { 1, 0, 16, {0x80,0,0,0xaa,0,0x38,0x9b,0x71} };

/* ---- events ---------------------------------------------------------- */
typedef struct { pthread_mutex_t m; pthread_cond_t c; int signaled; } fake_event_t;

HANDLE CreateEventA(void *sa, BOOL manual, BOOL initial, const char *name)
{
   fake_event_t *e = (fake_event_t*)calloc(1, sizeof(*e));
   (void)sa; (void)manual; (void)name;
   pthread_mutex_init(&e->m, NULL);
   pthread_cond_init(&e->c, NULL);
   e->signaled = initial ? 1 : 0;
   return e;
}
BOOL CloseHandle(HANDLE h)
{
   fake_event_t *e = (fake_event_t*)h;
   if (!e) return FALSE;
   pthread_cond_destroy(&e->c);
   pthread_mutex_destroy(&e->m);
   free(e);
   return TRUE;
}
BOOL SetEvent(HANDLE h)
{
   fake_event_t *e = (fake_event_t*)h;
   pthread_mutex_lock(&e->m);
   e->signaled = 1;
   pthread_cond_signal(&e->c);
   pthread_mutex_unlock(&e->m);
   return TRUE;
}
DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
   fake_event_t *e = (fake_event_t*)h;
   DWORD r = WAIT_TIMEOUT;
   struct timespec dl;
   clock_gettime(CLOCK_REALTIME, &dl);
   dl.tv_sec  += ms / 1000;
   dl.tv_nsec += (long)(ms % 1000) * 1000000L;
   if (dl.tv_nsec >= 1000000000L) { dl.tv_sec++; dl.tv_nsec -= 1000000000L; }
   pthread_mutex_lock(&e->m);
   while (!e->signaled)
   {
      int rc;
      if (ms == INFINITE)
         rc = pthread_cond_wait(&e->c, &e->m);
      else
         rc = pthread_cond_timedwait(&e->c, &e->m, &dl);
      if (rc == ETIMEDOUT)
         break;
   }
   if (e->signaled) { e->signaled = 0; r = WAIT_OBJECT_0; } /* auto-reset */
   pthread_mutex_unlock(&e->m);
   return r;
}
void Sleep(DWORD ms) { struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
void CoTaskMemFree(void *p) { free(p); }

/* ---- the device ------------------------------------------------------ */
static struct
{
   unsigned rate;
   REFERENCE_TIME min_period, default_period;
   bool accept_float;
   unsigned engine_min_frames, locked_period_frames;
} g_cfg = { 48000, 30000, 100000, false, 0, 0 };

void fake_device_configure_engine(unsigned engine_min_frames, unsigned locked_period_frames)
{
   g_cfg.engine_min_frames    = engine_min_frames;
   g_cfg.locked_period_frames = locked_period_frames;
}

typedef struct fake_client
{
   IAudioClient       client;
   IAudioClient3      client3;
   IAudioRenderClient render;
   unsigned           refs;
   bool               initialised, running;
   AUDCLNT_SHAREMODE  mode;
   REFERENCE_TIME     period_hns;
   unsigned           buffer_frames;   /* endpoint buffer */
   unsigned           period_frames;
   unsigned           frame_bytes;
   HANDLE             event;
   BYTE              *buffer;
   pthread_t          thread;
   pthread_mutex_t    m;
   /* exclusive: released since the last period? shared: padding. */
   bool               released;
   unsigned           padding;
   unsigned           got_frames;      /* frames handed out by GetBuffer */
   fake_device_stats_t stats;
} fake_client_t;

static fake_client_t *g_last = NULL;

void fake_device_configure(unsigned rate, REFERENCE_TIME min_period_hns,
      REFERENCE_TIME default_period_hns, bool accept_float)
{
   g_cfg.rate = rate; g_cfg.min_period = min_period_hns;
   g_cfg.default_period = default_period_hns; g_cfg.accept_float = accept_float;
}
void fake_device_stats(fake_device_stats_t *out)
{
   memset(out, 0, sizeof(*out));
   if (!g_last) return;
   pthread_mutex_lock(&g_last->m);
   *out = g_last->stats;
   pthread_mutex_unlock(&g_last->m);
}

static void *device_thread(void *p)
{
   fake_client_t *c = (fake_client_t*)p;
   struct timespec next;
   long period_ns = (long)(c->period_hns * 100);
   clock_gettime(CLOCK_MONOTONIC, &next);
   for (;;)
   {
      next.tv_nsec += period_ns;
      while (next.tv_nsec >= 1000000000L) { next.tv_sec++; next.tv_nsec -= 1000000000L; }
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      pthread_mutex_lock(&c->m);
      if (!c->running) { pthread_mutex_unlock(&c->m); break; }
      c->stats.periods++;
      if (c->mode == AUDCLNT_SHAREMODE_EXCLUSIVE)
      {
         if (c->released) { c->stats.frames_consumed += c->period_frames; c->released = false; }
         else c->stats.periods_unanswered++;
      }
      else
      {
         unsigned take = c->period_frames;
         if (c->padding >= take) { c->padding -= take; c->stats.frames_consumed += take; }
         else { c->stats.frames_consumed += c->padding; c->padding = 0; c->stats.periods_unanswered++; }
      }
      pthread_mutex_unlock(&c->m);
      if (c->event) SetEvent(c->event);
   }
   return NULL;
}

/* IAudioClient */
static HRESULT c_qi(IAudioClient *t, REFIID iid, void **out)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   if (g_cfg.engine_min_frames && memcmp(iid, &mmdevice_IID_IAudioClient3, sizeof(GUID)) == 0)
   {
      c->refs++;
      *out = &c->client3;
      return S_OK;
   }
   *out = NULL;
   return E_NOINTERFACE;
}
static DWORD   c_addref(IAudioClient *t) { return ++((fake_client_t*)t->fake)->refs; }
static DWORD   c_release(IAudioClient *t)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   if (--c->refs) return c->refs;
   if (c->running) { pthread_mutex_lock(&c->m); c->running = false; pthread_mutex_unlock(&c->m); pthread_join(c->thread, NULL); }
   free(c->buffer);
   pthread_mutex_destroy(&c->m);
   if (g_last == c) g_last = NULL;
   free(c);
   return 0;
}
static HRESULT c_initialize(IAudioClient *t, AUDCLNT_SHAREMODE mode, DWORD flags,
      REFERENCE_TIME dur, REFERENCE_TIME per, const WAVEFORMATEX *fmt, const GUID *g)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   (void)g;
   if (c->initialised) return AUDCLNT_E_ALREADY_INITIALIZED;
   if (mode == AUDCLNT_SHAREMODE_EXCLUSIVE)
   {
      if (!(flags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)) return E_FAIL;
      if (dur != per) return AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL;
      if (per < g_cfg.min_period) return AUDCLNT_E_INVALID_DEVICE_PERIOD;
      if (fmt->wFormatTag != WAVE_FORMAT_PCM && !g_cfg.accept_float) return AUDCLNT_E_UNSUPPORTED_FORMAT;
      c->period_hns    = per;
      c->period_frames = (unsigned)((per * g_cfg.rate + 5000000) / 10000000);
      c->buffer_frames = c->period_frames;
   }
   else
   {
      c->period_hns    = g_cfg.default_period;
      c->period_frames = (unsigned)((c->period_hns * g_cfg.rate + 5000000) / 10000000);
      /* The legacy shared engine buffer: the duration asked for, at
       * least two periods. */
      c->buffer_frames = (unsigned)((dur * g_cfg.rate + 5000000) / 10000000);
      if (c->buffer_frames < c->period_frames * 2) c->buffer_frames = c->period_frames * 2;
      /* Windows rounds the legacy shared buffer up to a multiple of 32
       * frames, and a little beyond: a 20 ms request came back 1056
       * frames (22 ms) on an NVIDIA HDMI endpoint. */
      c->buffer_frames = ((c->buffer_frames + 96) / 32) * 32;
   }
   c->mode        = mode;
   c->frame_bytes = fmt->nBlockAlign;
   c->buffer      = (BYTE*)calloc(c->buffer_frames, c->frame_bytes);
   c->initialised = true;
   c->stats.period_frames = c->period_frames;
   c->stats.buffer_frames = c->buffer_frames;
   c->stats.share_mode    = mode == AUDCLNT_SHAREMODE_EXCLUSIVE;
   c->stats.period_hns    = c->period_hns;
   return S_OK;
}
static HRESULT c_getbuffersize(IAudioClient *t, UINT32 *n) { *n = ((fake_client_t*)t->fake)->buffer_frames; return S_OK; }
static HRESULT c_getstreamlatency(IAudioClient *t, REFERENCE_TIME *l) { *l = ((fake_client_t*)t->fake)->period_hns; return S_OK; }
static HRESULT c_getpadding(IAudioClient *t, UINT32 *p)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   pthread_mutex_lock(&c->m); *p = c->padding; pthread_mutex_unlock(&c->m);
   return S_OK;
}
static HRESULT c_isformatsupported(IAudioClient *t, AUDCLNT_SHAREMODE mode, const WAVEFORMATEX *fmt, WAVEFORMATEX **closest)
{
   (void)t;
   if (closest) *closest = NULL;
   if (mode == AUDCLNT_SHAREMODE_EXCLUSIVE)
      return (fmt->wFormatTag == WAVE_FORMAT_PCM || g_cfg.accept_float) ? S_OK : AUDCLNT_E_UNSUPPORTED_FORMAT;
   return S_OK; /* shared: the engine mixes anything */
}
static HRESULT c_getmixformat(IAudioClient *t, WAVEFORMATEX **f) { (void)t; *f = NULL; return E_FAIL; }
static HRESULT c_getdeviceperiod(IAudioClient *t, REFERENCE_TIME *d, REFERENCE_TIME *m)
{
   (void)t;
   if (d) *d = g_cfg.default_period;
   if (m) *m = g_cfg.min_period;
   return S_OK;
}
static HRESULT c_start(IAudioClient *t)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   if (!c->initialised) return AUDCLNT_E_NOT_INITIALIZED;
   if (c->running) return AUDCLNT_E_NOT_STOPPED;
   c->running = true;
   g_last     = c;
   pthread_create(&c->thread, NULL, device_thread, c);
   return S_OK;
}
static HRESULT c_stop(IAudioClient *t)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   if (!c->running) return S_FALSE;
   pthread_mutex_lock(&c->m); c->running = false; pthread_mutex_unlock(&c->m);
   pthread_join(c->thread, NULL);
   return S_OK;
}
static HRESULT c_reset(IAudioClient *t) { fake_client_t *c = (fake_client_t*)t->fake; c->padding = 0; c->released = false; return S_OK; }
static HRESULT c_seteventhandle(IAudioClient *t, HANDLE h) { ((fake_client_t*)t->fake)->event = h; return S_OK; }
static HRESULT c_getservice(IAudioClient *t, REFIID iid, void **out)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   if (memcmp(iid, &IID_IAudioRenderClient, sizeof(GUID)) == 0) { c->refs++; *out = &c->render; return S_OK; }
   *out = NULL;
   return E_NOINTERFACE;
}
static const IAudioClientVtbl client_vtbl = {
   c_qi, c_addref, c_release, c_initialize, c_getbuffersize, c_getstreamlatency,
   c_getpadding, c_isformatsupported, c_getmixformat, c_getdeviceperiod,
   c_start, c_stop, c_reset, c_seteventhandle, c_getservice
};

/* IAudioClient3 */
static HRESULT c3_qi(IAudioClient3 *t, REFIID iid, void **out) { (void)t; (void)iid; *out = NULL; return E_NOINTERFACE; }
static DWORD   c3_addref(IAudioClient3 *t) { return ++((fake_client_t*)t->fake)->refs; }
static DWORD   c3_release(IAudioClient3 *t) { return c_release(&((fake_client_t*)t->fake)->client); }
static HRESULT c3_getperiods(IAudioClient3 *t, const WAVEFORMATEX *f, UINT32 *d, UINT32 *fu, UINT32 *mn, UINT32 *mx)
{
   (void)t; (void)f;
   *d  = (UINT32)((g_cfg.default_period * g_cfg.rate + 5000000) / 10000000);
   *fu = 48;
   *mn = g_cfg.engine_min_frames;
   *mx = *d;
   return S_OK;
}
static HRESULT c3_getcurrent(IAudioClient3 *t, WAVEFORMATEX **f, UINT32 *p)
{
   (void)t;
   *f = NULL;
   *p = g_cfg.locked_period_frames ? g_cfg.locked_period_frames
        : (UINT32)((g_cfg.default_period * g_cfg.rate + 5000000) / 10000000);
   return S_OK;
}
static HRESULT c3_init(IAudioClient3 *t, DWORD flags, UINT32 period, const WAVEFORMATEX *fmt, const GUID *g)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   (void)g;
   if (c->initialised) return AUDCLNT_E_ALREADY_INITIALIZED;
   if (!(flags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)) return E_FAIL;
   if (g_cfg.locked_period_frames && period != g_cfg.locked_period_frames)
      return AUDCLNT_E_ENGINE_PERIODICITY_LOCKED;
   c->mode          = AUDCLNT_SHAREMODE_SHARED;
   c->period_frames = period;
   c->period_hns    = (REFERENCE_TIME)period * 10000000 / g_cfg.rate;
   /* A small engine buffer, as the low-latency engine gives: three
    * periods. */
   c->buffer_frames = period * 3;
   c->frame_bytes   = fmt->nBlockAlign;
   c->buffer        = (BYTE*)calloc(c->buffer_frames, c->frame_bytes);
   c->initialised   = true;
   c->stats.period_frames = c->period_frames;
   c->stats.buffer_frames = c->buffer_frames;
   c->stats.share_mode    = 0;
   c->stats.period_hns    = c->period_hns;
   return S_OK;
}
static const IAudioClient3Vtbl client3_vtbl = { c3_qi, c3_addref, c3_release, c3_getperiods, c3_getcurrent, c3_init };

/* IAudioRenderClient */
static HRESULT r_qi(IAudioRenderClient *t, REFIID iid, void **out) { (void)t; (void)iid; *out = NULL; return E_NOINTERFACE; }
static DWORD   r_addref(IAudioRenderClient *t) { return ++((fake_client_t*)t->fake)->refs; }
static DWORD   r_release(IAudioRenderClient *t) { return c_release(&((fake_client_t*)t->fake)->client); }
static HRESULT r_getbuffer(IAudioRenderClient *t, UINT32 n, BYTE **pp)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   HRESULT hr = S_OK;
   pthread_mutex_lock(&c->m);
   if (c->mode == AUDCLNT_SHAREMODE_EXCLUSIVE ? n > c->buffer_frames : n > c->buffer_frames - c->padding)
      hr = AUDCLNT_E_BUFFER_SIZE_ERROR;
   else { *pp = c->buffer; c->got_frames = n; }
   pthread_mutex_unlock(&c->m);
   return hr;
}
static HRESULT r_releasebuffer(IAudioRenderClient *t, UINT32 n, DWORD flags)
{
   fake_client_t *c = (fake_client_t*)t->fake;
   (void)flags;
   pthread_mutex_lock(&c->m);
   c->stats.buffers_released++;
   if (c->mode == AUDCLNT_SHAREMODE_EXCLUSIVE) c->released = true;
   else c->padding += n;
   pthread_mutex_unlock(&c->m);
   return S_OK;
}
static const IAudioRenderClientVtbl render_vtbl = { r_qi, r_addref, r_release, r_getbuffer, r_releasebuffer };

/* IMMDevice */
static HRESULT d_qi(IMMDevice *t, REFIID iid, void **out) { (void)t; (void)iid; *out = NULL; return E_NOINTERFACE; }
static DWORD   d_addref(IMMDevice *t) { (void)t; return 2; }
static DWORD   d_release(IMMDevice *t) { (void)t; return 1; }
static HRESULT d_activate(IMMDevice *t, REFIID iid, DWORD ctx, void *pa, void **out)
{
   fake_client_t *c;
   (void)t; (void)ctx; (void)pa;
   if (memcmp(iid, &IID_IAudioClient, sizeof(GUID)) != 0) { *out = NULL; return E_NOINTERFACE; }
   c = (fake_client_t*)calloc(1, sizeof(*c));
   c->client.lpVtbl  = &client_vtbl;  c->client.fake  = c;
   c->client3.lpVtbl = &client3_vtbl; c->client3.fake = c;
   c->render.lpVtbl  = &render_vtbl;  c->render.fake  = c;
   c->refs = 1;
   pthread_mutex_init(&c->m, NULL);
   *out = &c->client;
   return S_OK;
}
static const IMMDeviceVtbl device_vtbl = { d_qi, d_addref, d_release, d_activate };
static IMMDevice g_device = { &device_vtbl, NULL };

/* Counted, so a leak or double release shows in the test. */
static int g_com_refs = 0;
bool  mmdevice_com_init(void) { g_com_refs++; return true; }
void  mmdevice_com_uninit(bool init) { if (init) g_com_refs--; }
int   fake_com_refs(void) { return g_com_refs; }
void *mmdevice_init_device(const char *id, unsigned data_flow) { (void)id; (void)data_flow; return &g_device; }
const char *mmdevice_hresult_name(int hr)
{
   static char buf[32];
   snprintf(buf, sizeof(buf), "0x%08x", (unsigned)hr);
   return buf;
}
void *mmdevice_list_new(const void *u, unsigned data_flow) { (void)u; (void)data_flow; return NULL; }
char *mmdevice_name(void *data) { (void)data; return NULL; }
void  mmdevice_thread(void *data) { (void)data; }

DWORD IMMNotificationThreadId = 0;
BOOL PostThreadMessage(DWORD id, unsigned msg, unsigned long wp, long lp) { (void)id; (void)msg; (void)wp; (void)lp; return TRUE; }

DWORD FormatMessageA(DWORD flags, const void *src, DWORD id, DWORD lang, LPSTR buf, DWORD size, void *args)
{
   (void)flags; (void)src; (void)lang; (void)args;
   return (DWORD)snprintf(buf, size, "error %u", (unsigned)id);
}
DWORD GetLastError(void) { return 0; }

HANDLE GetCurrentThread(void) { return NULL; }
BOOL SetThreadPriority(HANDLE h, int prio) { (void)h; (void)prio; return TRUE; }

/* audio/drivers/aaudio.c against a scripted libaaudio behind
 * dylib_load: the link carries no AAudio library, so the driver only
 * reaches the device through the symbols it resolves at runtime.
 *
 * Covers the open sequence (format, rate, channel mask on API 32 and
 * the count-only path before it, device selection and its fallback),
 * the ring and device-stage sizing, data reaching the device in order,
 * underruns, the lend pair, pause/resume, a device that stops calling
 * back, and a disconnect - which must free the writer and ask the
 * frontend to reopen, while a late report from a stream already gone
 * must not.
 *
 *   aaudio_test         the suite
 *   aaudio_test nolib   libaaudio.so absent: init fails cleanly */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <dynamic/dylib.h>
#include <rthreads/rthreads.h>
#include <lists/string_list.h>
#include <formats/iec61937.h>

#include "../../../configuration.h"
#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_aaudio;
extern size_t   stub_device_latency;
extern uint32_t stub_requested_layout;
extern int32_t  stub_caps_id;
extern unsigned stub_caps_channels;
extern bool     stub_caps_ac3;

static unsigned failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the scripted library ---------------------------------------- */

typedef int32_t (*data_cb_t)(void*, void*, void*, int32_t);
typedef void    (*err_cb_t)(void*, void*, int32_t);

#define MOCK_ERR_ILLEGAL      (-898)
#define MOCK_ERR_DISCONNECTED (-899)
#define ST_OPEN    2
#define ST_STARTED 4
#define ST_PAUSED  6
#define ST_STOPPED 10
#define ST_CLOSED  12

typedef struct mock_builder
{
   data_cb_t data_cb;
   void     *data_user;
   err_cb_t  err_cb;
   void     *err_user;
   uint32_t  mask;
   int32_t   device_id;
   int32_t   rate;
   int32_t   channels;
   int32_t   format;
   int32_t   sharing;
   int32_t   perf;
   int32_t   usage;
   int32_t   direction;
   bool      mask_set;
} mock_builder_t;

typedef struct mock_stream
{
   mock_builder_t b;
   uint32_t granted_mask;
   int32_t  granted_channels;
   int32_t  state;
   int32_t  buffer_size;
   int      starts, pauses, stops;
} mock_stream_t;

static int      mock_api_level   = 32;
static bool     mock_lib_missing;
static int32_t  mock_refuse_device;
static bool     mock_refuse_mask;
static int32_t  mock_burst       = 192;
/* Frames the device asks for per callback; the burst unless set. */
static int32_t  mock_cb_frames;
/* The output takes IEC 61937 (API 34 and a sink that decodes AC-3). */
static bool     mock_iec_ok;
#define MOCK_DEFAULT_DEVICE 3
static int32_t  mock_capacity    = 4096;
static int      mock_opens, mock_closes;
static mock_builder_t mock_last;

/* The device thread: runs the data callback a burst at a time while
 * the stream is started, under a lock requestStop and close take, as
 * AAudio's own stop does. */
static slock_t           *dev_lock;
static sthread_t         *dev_thread;
static retro_atomic_int_t dev_run;
static bool               dev_pump;
static mock_stream_t     *dev_stream;
static uint8_t            dev_capture[1 << 20];
static size_t             dev_captured;

static int32_t mk_create(mock_builder_t **b)
{
   *b = (mock_builder_t*)calloc(1, sizeof(**b));
   return *b ? 0 : -1;
}
static int32_t mk_delete(mock_builder_t *b) { free(b); return 0; }
static void mk_dev(mock_builder_t *b, int32_t v)  { b->device_id = v; }
static void mk_rate(mock_builder_t *b, int32_t v) { b->rate = v; }
static void mk_ch(mock_builder_t *b, int32_t v)   { b->channels = v; }
static void mk_fmt(mock_builder_t *b, int32_t v)  { b->format = v; }
static void mk_share(mock_builder_t *b, int32_t v){ b->sharing = v; }
static void mk_dir(mock_builder_t *b, int32_t v)  { b->direction = v; }
static void mk_perf(mock_builder_t *b, int32_t v) { b->perf = v; }
static void mk_usage(mock_builder_t *b, int32_t v){ b->usage = v; }
static void mk_ctype(mock_builder_t *b, int32_t v){ (void)b; (void)v; }
static void mk_mask(mock_builder_t *b, uint32_t v){ b->mask = v; b->mask_set = true; }
static void mk_data(mock_builder_t *b, data_cb_t cb, void *u) { b->data_cb = cb; b->data_user = u; }
static void mk_err(mock_builder_t *b, err_cb_t cb, void *u)   { b->err_cb = cb; b->err_user = u; }

static unsigned popcount32(uint32_t m)
{
   unsigned n = 0;
   for (; m; m &= m - 1)
      n++;
   return n;
}

static int32_t mk_open(mock_builder_t *b, mock_stream_t **out)
{
   mock_stream_t *s;
   mock_opens++;
   *out = NULL;
   if (b->device_id && b->device_id == mock_refuse_device)
      return MOCK_ERR_ILLEGAL;
   if (b->mask_set && mock_refuse_mask)
      return MOCK_ERR_ILLEGAL;
   if (b->format == 5 && (!mock_iec_ok || b->channels != 2 || b->rate != 48000))
      return MOCK_ERR_ILLEGAL;
   if (!(s = (mock_stream_t*)calloc(1, sizeof(*s))))
      return -1;
   s->b     = *b;
   s->state = ST_OPEN;
   if (b->mask_set)
   {
      s->granted_mask     = b->mask;
      s->granted_channels = (int32_t)popcount32(b->mask);
   }
   else
   {
      s->granted_mask     = 0xFFFFFFFFu;
      s->granted_channels = b->channels ? b->channels : 2;
   }
   if (!s->b.format)
      s->b.format = 2;
   if (!s->b.rate)
      s->b.rate   = 48000;
   if (!s->b.device_id)
      s->b.device_id = MOCK_DEFAULT_DEVICE;
   s->buffer_size = mock_capacity;
   /* What the driver ended up with: refused opens are not recorded. */
   mock_last = *b;
   *out = s;
   return 0;
}

static int32_t mk_close(mock_stream_t *s)
{
   slock_lock(dev_lock);
   if (dev_stream == s)
      dev_stream = NULL;
   slock_unlock(dev_lock);
   mock_closes++;
   free(s);
   return 0;
}
static int32_t mk_start(mock_stream_t *s)
{
   slock_lock(dev_lock);
   s->starts++;
   s->state   = ST_STARTED;
   dev_stream = s;
   slock_unlock(dev_lock);
   return 0;
}
static int32_t mk_pause(mock_stream_t *s)
{
   slock_lock(dev_lock);
   s->pauses++;
   s->state = ST_PAUSED;
   slock_unlock(dev_lock);
   return 0;
}
static int32_t mk_stop(mock_stream_t *s)
{
   slock_lock(dev_lock);
   s->stops++;
   s->state = ST_STOPPED;
   slock_unlock(dev_lock);
   return 0;
}
static int32_t mk_wait(mock_stream_t *s, int32_t in, int32_t *next, int64_t ns)
{
   (void)in; (void)ns;
   *next = s->state;
   return 0;
}
static int32_t mk_set_bufsize(mock_stream_t *s, int32_t f)
{
   if (f > mock_capacity)
      f = mock_capacity;
   s->buffer_size = f;
   return f;
}
static int32_t mk_get_bufsize(mock_stream_t *s) { return s->buffer_size; }
static int32_t mk_get_cap(mock_stream_t *s)     { (void)s; return mock_capacity; }
static int32_t mk_get_burst(mock_stream_t *s)   { (void)s; return mock_burst; }
static int32_t mk_get_rate(mock_stream_t *s)    { return s->b.rate; }
static int32_t mk_get_ch(mock_stream_t *s)      { return s->granted_channels; }
static int32_t mk_get_fmt(mock_stream_t *s)     { return s->b.format; }
static int32_t mk_get_share(mock_stream_t *s)   { return s->b.sharing; }
static int32_t mk_get_perf(mock_stream_t *s)    { return s->b.perf; }
static int32_t mk_get_state(mock_stream_t *s)   { return s->state; }
static int32_t mk_get_dev(mock_stream_t *s)     { return s->b.device_id; }
static uint32_t mk_get_mask(mock_stream_t *s)   { return s->granted_mask; }
static const char *mk_text(int32_t r)           { (void)r; return "mock error"; }

static size_t frame_bytes_of(const mock_stream_t *s)
{
   return (size_t)s->granted_channels * (s->b.format == 2 ? 4 : 2);
   /* format 5, the IEC 61937 carrier, is 2 x 16 bits */
}

/* One callback of the current stream, if started. The device thread
 * passes its pump flag through, read under the same lock. */
static int32_t dev_pump_locked(bool from_thread)
{
   uint8_t buf[192 * 8 * 4];
   int32_t n = 0;
   int32_t want;
   slock_lock(dev_lock);
   want = mock_cb_frames ? mock_cb_frames : mock_burst;
   if (     (!from_thread || dev_pump)
         && dev_stream && dev_stream->state == ST_STARTED && dev_stream->b.data_cb)
   {
      size_t fb  = frame_bytes_of(dev_stream);
      size_t len = (size_t)want * fb;
      if (len > sizeof(buf))
         len = sizeof(buf);
      n = (int32_t)(len / fb);
      dev_stream->b.data_cb(dev_stream, dev_stream->b.data_user, buf, n);
      if (dev_captured + len <= sizeof(dev_capture))
      {
         memcpy(dev_capture + dev_captured, buf, len);
         dev_captured += len;
      }
   }
   slock_unlock(dev_lock);
   return n;
}

static int32_t dev_pump_once(void) { return dev_pump_locked(false); }

static void dev_set_cb_frames(int32_t frames)
{
   slock_lock(dev_lock);
   mock_cb_frames = frames;
   slock_unlock(dev_lock);
}

static void dev_set_pump(bool on)
{
   slock_lock(dev_lock);
   dev_pump = on;
   slock_unlock(dev_lock);
}

static size_t dev_get_captured(void)
{
   size_t n;
   slock_lock(dev_lock);
   n = dev_captured;
   slock_unlock(dev_lock);
   return n;
}

static void dev_thread_fn(void *u)
{
   (void)u;
   while (retro_atomic_load_acquire_int(&dev_run))
   {
      dev_pump_locked(true);
      retro_sleep(1);
   }
}

/* A disconnect, on a thread of its own as AAudio raises it. */
static void dev_disconnect(const mock_builder_t *b)
{
   b->err_cb(NULL, b->err_user, MOCK_ERR_DISCONNECTED);
}

/* ---- dylib, as the driver sees it ---------------------------------- */

#define SYM(n, f) { n, (function_t)f, 0 }
static const struct { const char *name; function_t fn; int api; } syms[] = {
   SYM("AAudio_createStreamBuilder",             mk_create),
   SYM("AAudioStreamBuilder_delete",             mk_delete),
   SYM("AAudioStreamBuilder_openStream",         mk_open),
   SYM("AAudioStreamBuilder_setDeviceId",        mk_dev),
   SYM("AAudioStreamBuilder_setSampleRate",      mk_rate),
   SYM("AAudioStreamBuilder_setChannelCount",    mk_ch),
   SYM("AAudioStreamBuilder_setFormat",          mk_fmt),
   SYM("AAudioStreamBuilder_setSharingMode",     mk_share),
   SYM("AAudioStreamBuilder_setDirection",       mk_dir),
   SYM("AAudioStreamBuilder_setPerformanceMode", mk_perf),
   SYM("AAudioStreamBuilder_setDataCallback",    mk_data),
   SYM("AAudioStreamBuilder_setErrorCallback",   mk_err),
   SYM("AAudioStream_close",                     mk_close),
   SYM("AAudioStream_requestStart",              mk_start),
   SYM("AAudioStream_requestPause",              mk_pause),
   SYM("AAudioStream_requestStop",               mk_stop),
   SYM("AAudioStream_waitForStateChange",        mk_wait),
   SYM("AAudioStream_setBufferSizeInFrames",     mk_set_bufsize),
   SYM("AAudioStream_getBufferSizeInFrames",     mk_get_bufsize),
   SYM("AAudioStream_getBufferCapacityInFrames", mk_get_cap),
   SYM("AAudioStream_getFramesPerBurst",         mk_get_burst),
   SYM("AAudioStream_getSampleRate",             mk_get_rate),
   SYM("AAudioStream_getChannelCount",           mk_get_ch),
   SYM("AAudioStream_getFormat",                 mk_get_fmt),
   SYM("AAudioStream_getSharingMode",            mk_get_share),
   SYM("AAudioStream_getPerformanceMode",        mk_get_perf),
   SYM("AAudioStream_getState",                  mk_get_state),
   SYM("AAudioStream_getDeviceId",               mk_get_dev),
   SYM("AAudio_convertResultToText",             mk_text),
   { "AAudioStreamBuilder_setUsage",       (function_t)mk_usage, 28 },
   { "AAudioStreamBuilder_setContentType", (function_t)mk_ctype, 28 },
   { "AAudioStreamBuilder_setChannelMask", (function_t)mk_mask,  32 },
   { "AAudioStream_getChannelMask",        (function_t)mk_get_mask, 32 },
   { NULL, NULL, 0 }
};

static int mock_lib_handle;

dylib_t dylib_load(const char *path)
{
   if (mock_lib_missing || strcmp(path, "libaaudio.so"))
      return NULL;
   return &mock_lib_handle;
}

function_t dylib_proc(dylib_t lib, const char *name)
{
   int i;
   if (lib != &mock_lib_handle)
      return NULL;
   for (i = 0; syms[i].name; i++)
      if (!strcmp(syms[i].name, name))
         return syms[i].api > mock_api_level ? NULL : syms[i].fn;
   return NULL;
}

void dylib_close(dylib_t lib) { (void)lib; }
char *dylib_error(void) { return NULL; }

/* ---- cases ---------------------------------------------------------- */

static void *open_driver(const char *device, unsigned latency, unsigned *rate)
{
   *rate = 0;
   return audio_aaudio.init(device, 44100, latency, rate);
}

static void case_open_defaults(void)
{
   unsigned rate;
   void *h;
   size_t fb, ring;

   printf("   open: float, native rate, low latency, two-burst device stage\n");
   h = open_driver(NULL, 64, &rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return;
   CHECK(mock_last.rate == 0, "a rate was forced (%d)", (int)mock_last.rate);
   CHECK(rate == 48000, "new_rate %u, not the device's 48000", rate);
   CHECK(mock_last.format == 2 && audio_aaudio.use_float(h), "not opened as float");
   CHECK(mock_last.perf == 12, "not low latency");
   CHECK(mock_last.sharing == 1, "not shared");
   CHECK(mock_last.direction == 0, "not output");
   CHECK(mock_last.usage == 14, "usage %d, not GAME", (int)mock_last.usage);
   CHECK(stub_device_latency == (size_t)mock_burst * 2,
         "device stage %u frames, not two bursts", (unsigned)stub_device_latency);
   fb   = 8;
   ring = audio_aaudio.buffer_size(h);
   CHECK(ring % fb == 0, "buffer_size %u is not whole frames", (unsigned)ring);
   /* 64 ms at 48 kHz, less what the device stage holds. */
   CHECK(ring / fb == 3072 - 384, "ring %u frames, not %u", (unsigned)(ring / fb), 3072u - 384u);
   CHECK(audio_aaudio.layout(h) == AUDIO_LAYOUT_STEREO, "layout 0x%x", audio_aaudio.layout(h));
   CHECK(audio_aaudio.alive(h), "not alive after init");
   /* Starts at the rate control's setpoint. */
   CHECK(audio_aaudio.write_avail(h) == ring - (ring / fb / 2) * fb,
         "write_avail %u at start, ring %u", (unsigned)audio_aaudio.write_avail(h), (unsigned)ring);
   audio_aaudio.free(h);

   printf("   open: a latency below the device stage keeps a two-burst ring\n");
   h = open_driver(NULL, 2, &rate);
   CHECK(h && audio_aaudio.buffer_size(h) == (size_t)mock_burst * 2 * 8,
         "ring %u bytes", h ? (unsigned)audio_aaudio.buffer_size(h) : 0);
   if (h)
      audio_aaudio.free(h);
}

static void case_int16(void)
{
   unsigned rate;
   void *h;
   printf("   open: 16-bit when the negotiation hint asks for it\n");
   config_get_ptr()->uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_INT16;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format == 1 && !audio_aaudio.use_float(h), "not opened as 16-bit");
   CHECK(h && audio_aaudio.buffer_size(h) % 4 == 0, "ring not in 16-bit stereo frames");
   if (h)
      audio_aaudio.free(h);
   config_get_ptr()->uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_FLOAT;
}

static void case_layouts(void)
{
   unsigned rate;
   void *h;
   uint32_t side51 = AUDIO_LAYOUT_5POINT1_SURROUND;

   stub_requested_layout = side51;

   printf("   layout: API 32 takes the mask as asked\n");
   mock_api_level = 32;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.mask_set && mock_last.mask == side51, "mask not passed");
   CHECK(h && audio_aaudio.layout(h) == side51, "layout 0x%x", h ? audio_aaudio.layout(h) : 0);
   CHECK(h && audio_aaudio.buffer_size(h) % 24 == 0, "ring not in 6-channel float frames");
   if (h)
      audio_aaudio.free(h);

   printf("   layout: a refused mask falls back to the count\n");
   mock_refuse_mask = true;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && !mock_last.mask_set && mock_last.channels == 6, "count path not taken");
   CHECK(h && audio_aaudio.layout(h) == AUDIO_LAYOUT_5POINT1,
         "layout 0x%x, not the count's canonical 5.1", h ? audio_aaudio.layout(h) : 0);
   if (h)
      audio_aaudio.free(h);
   mock_refuse_mask = false;
   /* The library is resolved once per process, so a lower API level is
    * modelled by the mask being refused, as above; the count path and
    * its canonical positions are the same code. */

   printf("   layout: past what the frontend has, stereo\n");
   stub_requested_layout = 0x1000u | AUDIO_LAYOUT_STEREO;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && audio_aaudio.layout(h) == AUDIO_LAYOUT_STEREO, "not stereo");
   CHECK(h && !mock_last.mask_set && mock_last.channels == 2, "not opened as stereo");
   if (h)
      audio_aaudio.free(h);

   stub_requested_layout = AUDIO_LAYOUT_STEREO;
}

static void case_devices(void)
{
   unsigned rate;
   void *h;

   printf("   device: the id at the front of the entry\n");
   h = open_driver("7 (USB DAC, USB device)", 64, &rate);
   CHECK(h && mock_last.device_id == 7, "device %d", (int)mock_last.device_id);
   if (h)
      audio_aaudio.free(h);

   printf("   device: a refused device falls back to the default\n");
   mock_refuse_device = 9;
   mock_opens = 0;
   h = open_driver("9 (Gone, Bluetooth A2DP)", 64, &rate);
   CHECK(h != NULL, "no fallback");
   CHECK(mock_last.device_id == 0 && mock_opens == 2,
         "device %d after %d opens", (int)mock_last.device_id, mock_opens);
   if (h)
      audio_aaudio.free(h);
   mock_refuse_device = 0;

   printf("   device: a name that is not an id opens the default\n");
   h = open_driver("default", 64, &rate);
   CHECK(h && mock_last.device_id == 0, "device %d", (int)mock_last.device_id);
   if (h)
      audio_aaudio.free(h);
}

static void case_stream(void)
{
   unsigned rate;
   size_t   i, ring, prefill, sent = 0, consumed;
   float    ramp[4096];
   void    *h;
   ssize_t  w;
   int      tries;
   bool     in_order = true;

   printf("   stream: blocking writes reach the device in order\n");
   h = open_driver(NULL, 32, &rate);
   if (!h)
   {
      CHECK(0, "init failed");
      return;
   }
   ring    = audio_aaudio.buffer_size(h);
   prefill = ring - audio_aaudio.write_avail(h);
   for (i = 0; i < sizeof(ramp) / sizeof(ramp[0]); i++)
      ramp[i] = (float)(i + 1);

   slock_lock(dev_lock);
   dev_captured = 0;
   slock_unlock(dev_lock);
   dev_set_pump(true);
   audio_aaudio.set_nonblock_state(h, false);
   for (i = 0; i < 8; i++)
   {
      w = audio_aaudio.write(h, ramp, sizeof(ramp));
      CHECK(w == (ssize_t)sizeof(ramp), "write %u returned %ld", (unsigned)i, (long)w);
      if (w > 0)
         sent += (size_t)w;
   }
   /* Let the device drain what was written. */
   for (tries = 0; tries < 2000 && dev_get_captured() < prefill + sent; tries++)
      retro_sleep(1);
   /* Off under the lock, so nothing pumps from here on. */
   dev_set_pump(false);

   CHECK(dev_captured >= prefill + sent, "device took %u of %u bytes",
         (unsigned)dev_captured, (unsigned)(prefill + sent));
   for (i = 0; i < prefill && i < dev_captured; i++)
      if (dev_capture[i])
      {
         CHECK(0, "prefill byte %u is not silence", (unsigned)i);
         break;
      }
   for (i = 0; prefill + (i + 1) * sizeof(float) <= dev_captured
         && i < sent / sizeof(float); i++)
   {
      float v;
      memcpy(&v, dev_capture + prefill + i * sizeof(float), sizeof(v));
      if (v != ramp[i % (sizeof(ramp) / sizeof(ramp[0]))])
      {
         in_order = false;
         break;
      }
   }
   CHECK(in_order, "sample %u out of order", (unsigned)i);

   consumed = audio_aaudio.frames_consumed(h);
   CHECK(consumed * 8 == dev_captured, "frames_consumed %u, device took %u frames",
         (unsigned)consumed, (unsigned)(dev_captured / 8));

   printf("   stream: an empty ring pads with silence and counts it\n");
   {
      size_t before = audio_aaudio.underruns(h);
      size_t n      = 0;
      while (audio_aaudio.write_avail(h) < ring && n++ < 10000)
         dev_pump_once();
      dev_pump_once();
      dev_pump_once();
      CHECK(audio_aaudio.underruns(h) > before, "no underrun counted");
   }

   printf("   stream: nonblocking writes take what fits\n");
   audio_aaudio.set_nonblock_state(h, true);
   {
      size_t room = audio_aaudio.write_avail(h);
      size_t got  = 0;
      while ((w = audio_aaudio.write(h, ramp, sizeof(ramp))) > 0)
         got += (size_t)w;
      CHECK(w == 0, "a full ring returned %ld", (long)w);
      CHECK(got == room, "took %u of %u bytes of room", (unsigned)got, (unsigned)room);
      CHECK(audio_aaudio.write_avail(h) == 0, "room left after filling");
   }

   printf("   stream: a device that stops calling back ends the wait\n");
   audio_aaudio.set_nonblock_state(h, false);
   w = audio_aaudio.write(h, ramp, 64);
   CHECK(w == 0, "a write to a stalled full ring returned %ld", (long)w);
   CHECK(audio_aaudio.wait_writable(h, 64) == 0, "wait_writable on a stall");

   printf("   stream: the lend pair, in whole frames\n");
   {
      void  *region = NULL;
      size_t span;
      dev_pump_once();
      dev_pump_once();
      span = audio_aaudio.write_begin(h, 1003, &region);
      CHECK(span > 0 && region, "nothing lent");
      CHECK(span % 8 == 0 && span <= 1003, "span %u", (unsigned)span);
      if (span)
      {
         memset(region, 0, span);
         CHECK(audio_aaudio.write_end(h, span) == (ssize_t)span, "write_end");
      }
   }

   printf("   stream: pause and resume\n");
   CHECK(audio_aaudio.stop(h) && !audio_aaudio.alive(h), "not paused");
   CHECK(dev_stream && dev_stream->pauses == 1, "requestPause not called");
   CHECK(audio_aaudio.stop(h), "a second stop failed");
   CHECK(dev_stream && dev_stream->pauses == 1, "paused twice");
   CHECK(audio_aaudio.start(h, false) && audio_aaudio.alive(h), "not resumed");
   CHECK(dev_stream && dev_stream->starts == 2, "requestStart not called");

   mock_closes = 0;
   audio_aaudio.free(h);
   CHECK(mock_closes == 1, "%d closes at free", mock_closes);
}

static void case_disconnect(void)
{
   unsigned rate;
   mock_builder_t old;
   float    buf[512];
   void    *h;
   retro_atomic_int_t *reinit = &audio_state_get_ptr()->reinit_request;

   memset(buf, 0, sizeof(buf));
   printf("   disconnect: the writer is freed and the frontend asked to reopen\n");
   retro_atomic_store_release_int(reinit, 0);
   h = open_driver(NULL, 32, &rate);
   if (!h)
   {
      CHECK(0, "init failed");
      return;
   }
   /* Full, and nothing draining it. */
   audio_aaudio.set_nonblock_state(h, true);
   while (audio_aaudio.write(h, buf, sizeof(buf)) > 0) ;
   audio_aaudio.set_nonblock_state(h, false);

   old = mock_last;
   dev_disconnect(&mock_last);
   CHECK(retro_atomic_load_acquire_int(reinit) == 1, "no reinit request");
   CHECK(audio_aaudio.write(h, buf, sizeof(buf)) == (ssize_t)sizeof(buf),
         "a write to a lost stream did not return at once");
   CHECK(audio_aaudio.wait_writable(h, 64) == 0, "wait_writable on a lost stream");
   CHECK(audio_aaudio.start(h, false), "start on a lost stream failed");
   audio_aaudio.free(h);

   printf("   disconnect: a late report from a closed stream is dropped\n");
   retro_atomic_store_release_int(reinit, 0);
   h = open_driver(NULL, 32, &rate);
   if (!h)
   {
      CHECK(0, "init failed");
      return;
   }
   dev_disconnect(&old);
   CHECK(retro_atomic_load_acquire_int(reinit) == 0, "a stale report asked for a reinit");
   CHECK(audio_aaudio.write(h, buf, 64) == 64, "a stale report broke the new stream");
   audio_aaudio.free(h);
}

static void case_device_list(void)
{
   struct string_list *list;
   printf("   device list: the platform's outputs\n");
   list = (struct string_list*)audio_aaudio.device_list_new(NULL);
   CHECK(list && list->size == 2, "no list");
   audio_aaudio.device_list_free(NULL, list);
}

/* The offset of the first burst of this type in what the device took,
 * at 64-byte steps from start; -1 if none. */
static long find_burst(size_t start, size_t end, unsigned want)
{
   size_t off;
   for (off = start; off + 8 <= end; off += IEC61937_PAUSE_BURST_BYTES)
   {
      unsigned type = 0;
      if (iec61937_probe(dev_capture + off, end - off, &type, NULL)
            && type == want)
         return (long)off;
   }
   return -1;
}

/* Walks what the device took from the first AC-3 burst: counts the
 * whole AC-3 bursts and says whether pause bursts follow the last.
 * False on anything that is not a burst on a boundary. */
static bool walk_bursts(size_t captured, unsigned *bursts, bool *gap_after)
{
   long   first = find_burst(0, captured, IEC61937_AC3);
   size_t off;
   *bursts    = 0;
   *gap_after = false;
   if (first < 0)
      return true;
   off = (size_t)first;
   while (off + 8 <= captured)
   {
      unsigned t = 0;
      if (!iec61937_probe(dev_capture + off, 8, &t, NULL))
         return false;
      if (t == IEC61937_AC3)
      {
         if (off + IEC61937_AC3_BURST_BYTES > captured)
            break;
         (*bursts)++;
         off       += IEC61937_AC3_BURST_BYTES;
         *gap_after = false;
      }
      else if (t == IEC61937_PAUSE)
      {
         off       += IEC61937_PAUSE_BURST_BYTES;
         *gap_after = true;
      }
      else
         return false;
   }
   return true;
}

static void case_ac3(void)
{
   unsigned rate;
   void    *h;
   size_t   cap, fb, i, captured;
   long     first;
   float    pcm[1536 * 6];
   ssize_t  w;
   int      tries;

   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i++)
      pcm[i] = (float)((int)(i % 97) - 48) / 64.0f;

   stub_requested_layout = AUDIO_LAYOUT_5POINT1_SURROUND;
   stub_caps_id          = MOCK_DEFAULT_DEVICE;
   stub_caps_channels    = 2;
   stub_caps_ac3         = true;
   mock_iec_ok           = true;

   printf("   ac3: 5.1 to a stereo-only output that decodes AC-3\n");
   h = open_driver(NULL, 64, &rate);
   if (!h)
   {
      CHECK(0, "init failed");
      goto out;
   }
   CHECK(mock_last.format == 5 && mock_last.channels == 2 && mock_last.rate == 48000,
         "not opened as IEC 61937 (format %d, %d ch, %d Hz)",
         (int)mock_last.format, (int)mock_last.channels, (int)mock_last.rate);
   CHECK(mock_last.device_id == MOCK_DEFAULT_DEVICE, "not on the output the PCM stream had");
   CHECK(rate == 48000, "new_rate %u", rate);
   CHECK(audio_aaudio.layout(h) == AUDIO_LAYOUT_5POINT1_SURROUND,
         "layout 0x%x", audio_aaudio.layout(h));
   CHECK(audio_aaudio.use_float(h), "the frontend does not write float");
   fb  = 6 * sizeof(float);
   cap = audio_aaudio.buffer_size(h);
   CHECK(cap % fb == 0 && (cap / fb) % 1536 == 0 && cap / fb >= 4 * 1536,
         "buffer_size %u is not whole bursts of 5.1 frames", (unsigned)cap);
   CHECK(audio_aaudio.write_begin(h, 4096, (void**)&w) == 0, "lent a span on the AC-3 path");

   printf("   ac3: the device starts on pause bursts, never zeros\n");
   dev_set_cb_frames(101);   /* cuts pause bursts across callbacks */
   slock_lock(dev_lock);
   dev_captured = 0;
   slock_unlock(dev_lock);
   dev_pump_once();
   dev_pump_once();
   CHECK(find_burst(0, dev_get_captured(), IEC61937_PAUSE) == 0, "no pause burst at the start");
   for (i = 64; i + 8 <= dev_get_captured(); i += 64)
   {
      unsigned t = 0;
      if (!iec61937_probe(dev_capture + i, 8, &t, NULL) || t != IEC61937_PAUSE)
      {
         CHECK(0, "pause bursts not back to back at byte %u", (unsigned)i);
         break;
      }
   }
   CHECK(audio_aaudio.underruns(h) == 0, "pause bursts before the first write counted as underruns");

   printf("   ac3: written audio reaches the device as whole AC-3 bursts\n");
   {
      size_t before = audio_aaudio.write_avail(h);
      w = audio_aaudio.write(h, pcm, 100 * fb);
      CHECK(w == (ssize_t)(100 * fb), "a part burst returned %ld", (long)w);
      CHECK(audio_aaudio.write_avail(h) == before - 100 * fb,
            "write_avail does not count frames in the encoder");
   }
   dev_set_pump(true);
   audio_aaudio.set_nonblock_state(h, false);
   for (i = 0; i < 6; i++)
   {
      w = audio_aaudio.write(h, pcm, sizeof(pcm));
      CHECK(w == (ssize_t)sizeof(pcm), "write %u returned %ld", (unsigned)i, (long)w);
   }
   /* Until the device has taken all six and run on into the gap. */
   for (tries = 0; tries < 5000; tries++)
   {
      unsigned n;
      bool     gap;
      dev_set_pump(false);
      if (!walk_bursts(dev_get_captured(), &n, &gap) || (n == 6 && gap))
         break;
      dev_set_pump(true);
      retro_sleep(1);
   }
   dev_set_pump(false);
   captured = dev_get_captured();
   first    = find_burst(0, captured, IEC61937_AC3);
   CHECK(first > 0 && first % 64 == 0, "first AC-3 burst at %ld", first);
   /* Every burst whole, on a boundary; where the writer fell behind
    * the device, pause bursts between them, never anything else. */
   {
      unsigned bursts = 0;
      bool     gap_after = false;
      CHECK(walk_bursts(captured, &bursts, &gap_after),
            "something other than whole bursts on a boundary");
      CHECK(bursts == 6, "%u whole AC-3 bursts, not 6", bursts);
      printf("   ac3: a gap after them is pause bursts, on the burst boundary\n");
      CHECK(gap_after, "no pause burst where the audio ran out");
      CHECK(audio_aaudio.underruns(h) > 0, "the gap was not counted");
   }
   CHECK(audio_aaudio.frames_consumed(h) * 4 == captured,
         "frames_consumed %u, device took %u carrier frames",
         (unsigned)audio_aaudio.frames_consumed(h), (unsigned)(captured / 4));

   printf("   ac3: nonblocking, a burst is taken whole or not at all\n");
   audio_aaudio.set_nonblock_state(h, true);
   {
      size_t got  = 0;
      int    laps = 0;
      while (laps++ < 64 && (w = audio_aaudio.write(h, pcm, sizeof(pcm))) > 0)
         got += (size_t)w;
      CHECK(laps < 64, "a full ring kept taking audio");
      CHECK(audio_aaudio.write_avail(h) < 1536 * fb, "room for a burst left in a full ring");
      CHECK(got % fb == 0, "took part of a frame");
   }
   audio_aaudio.free(h);
   dev_set_cb_frames(0);

   printf("   ac3: 7.1 goes as 5.1 with the pair at the sides\n");
   stub_requested_layout = AUDIO_LAYOUT_7POINT1;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && audio_aaudio.layout(h) == AUDIO_LAYOUT_5POINT1_SURROUND,
         "layout 0x%x", h ? audio_aaudio.layout(h) : 0);
   if (h)
      audio_aaudio.free(h);
   stub_requested_layout = AUDIO_LAYOUT_5POINT1_SURROUND;

   printf("   ac3: quad's back pair goes to the sides, as A/52 has it\n");
   stub_requested_layout = AUDIO_LAYOUT_QUAD;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && audio_aaudio.layout(h)
         == (AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_SIDE_LEFT | AUDIO_SPEAKER_SIDE_RIGHT),
         "layout 0x%x", h ? audio_aaudio.layout(h) : 0);
   if (h)
      audio_aaudio.free(h);
   stub_requested_layout = AUDIO_LAYOUT_5POINT1_SURROUND;

   printf("   ac3: PCM where the output takes the channels\n");
   stub_caps_channels = 8;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format == 2, "AC-3 to an 8-channel output");
   if (h)
      audio_aaudio.free(h);
   stub_caps_channels = 0;   /* lists none: any count */
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format == 2, "AC-3 to an output with any count");
   if (h)
      audio_aaudio.free(h);
   stub_caps_channels = 2;

   printf("   ac3: PCM where the output does not list AC-3\n");
   stub_caps_ac3 = false;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format == 2 && audio_aaudio.layout(h) == AUDIO_LAYOUT_5POINT1_SURROUND,
         "not 5.1 PCM");
   if (h)
      audio_aaudio.free(h);
   stub_caps_ac3 = true;

   printf("   ac3: PCM where IEC 61937 will not open (before Android 14)\n");
   mock_iec_ok = false;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format != 5 && audio_aaudio.layout(h) == AUDIO_LAYOUT_5POINT1_SURROUND,
         "no PCM fallback");
   CHECK(h && audio_aaudio.write_begin(h, 64, (void**)&w) > 0, "the PCM fallback lends nothing");
   if (h)
      audio_aaudio.free(h);

   printf("   ac3: stereo never goes as AC-3\n");
   mock_iec_ok = true;
   stub_requested_layout = AUDIO_LAYOUT_STEREO;
   h = open_driver(NULL, 64, &rate);
   CHECK(h && mock_last.format == 2, "stereo encoded");
   if (h)
      audio_aaudio.free(h);

out:
   dev_set_cb_frames(0);
   mock_iec_ok           = false;
   stub_caps_id          = 0;
   stub_requested_layout = AUDIO_LAYOUT_STEREO;
}

int main(int argc, char **argv)
{
   unsigned rate;

   if (argc > 1 && !strcmp(argv[1], "nolib"))
   {
      printf("aaudio_test nolib\n");
      mock_lib_missing = true;
      CHECK(audio_aaudio.init(NULL, 48000, 64, &rate) == NULL,
            "init without libaaudio.so");
      printf(failures ? "FAILED\n" : "PASS\n");
      return failures ? 1 : 0;
   }

   printf("aaudio_test\n");
   config_get_ptr()->uints.audio_format_negotiation = AUDIO_FORMAT_NEGOTIATION_FLOAT;
   dev_lock = slock_new();
   retro_atomic_int_init(&dev_run, 1);
   dev_thread = sthread_create(dev_thread_fn, NULL);
   if (!dev_lock || !dev_thread)
   {
      printf("could not start the device thread\n");
      return 1;
   }

   case_open_defaults();
   case_int16();
   case_layouts();
   case_devices();
   case_stream();
   case_disconnect();
   case_device_list();
   case_ac3();

   retro_atomic_store_release_int(&dev_run, 0);
   sthread_join(dev_thread);
   slock_free(dev_lock);

   printf(failures ? "FAILED (%u)\n" : "PASS\n", failures);
   return failures ? 1 : 0;
}

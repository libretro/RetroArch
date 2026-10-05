/* audio/drivers/jack.c reaches libjack only through the library it
 * loads itself. This links the shipping driver against no JACK library
 * at all - a direct reference to any jack_* symbol fails the link - and
 * stands in for dylib_load/dylib_proc/dylib_close with a fake libjack:
 *
 *  - absent: init and the device list fail cleanly, nothing is called;
 *  - each required entry point withheld in turn: the load fails, the
 *    handle is closed, the server is never asked for anything;
 *  - the optional latency query withheld: the driver comes up without
 *    it, reports no graph latency, and plays - samples written reach
 *    the port buffers through the process callback - and tears down
 *    with deactivate before close, so every table entry is checked
 *    against the name it was loaded from. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <dynamic/dylib.h>
#include <lists/string_list.h>

#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_jack;

/* From stubs_retroarch.c. */
extern size_t jr_device_latency;

static int failures;

#define CHECK(cond, msg) \
   do { if (!(cond)) { printf("[FAIL] %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

/* ---- the fake server ------------------------------------------------ */

struct _jack_client { int open; };
struct _jack_port   { char name[32]; float buf[1024]; };
struct jack_ringbuffer { char *buf; size_t size; size_t r; size_t w; };

static struct _jack_client fake_client;
static struct _jack_port   fake_ports[8];
static unsigned            fake_nports;
static int                 fake_open_fails;

static int n_open, n_close, n_activate, n_deactivate, n_connect, n_free;
static int n_rb_create, n_rb_free;
static int deactivated_before_close;
static JackProcessCallback fake_process;
static void               *fake_process_arg;

static jack_client_t *fake_client_open(const char *name,
      jack_options_t options, jack_status_t *status, ...)
{
   (void)name; (void)options;
   n_open++;
   if (status)
      *status = (jack_status_t)0;
   if (fake_open_fails)
   {
      if (status)
         *status = JackFailure;
      return NULL;
   }
   fake_client.open = 1;
   fake_nports      = 0;
   return &fake_client;
}

static int fake_client_close(jack_client_t *c)
{
   n_close++;
   deactivated_before_close = (n_deactivate > 0) || !fake_process;
   c->open = 0;
   return 0;
}

static int fake_activate(jack_client_t *c)   { (void)c; n_activate++; return 0; }
static int fake_deactivate(jack_client_t *c) { (void)c; n_deactivate++; return 0; }

static int fake_connect(jack_client_t *c, const char *src, const char *dst)
{
   (void)c; (void)src; (void)dst;
   n_connect++;
   return 0;
}

static void fake_free(void *p) { n_free++; free(p); }

static jack_nframes_t fake_get_buffer_size(jack_client_t *c) { (void)c; return 256; }
static jack_nframes_t fake_get_sample_rate(jack_client_t *c) { (void)c; return 48000; }

static int fake_get_cycle_times(const jack_client_t *c, jack_nframes_t *f,
      jack_time_t *u, jack_time_t *n, float *p)
{
   (void)c; (void)f; (void)u; (void)n; (void)p;
   return -1;
}

static const char **fake_get_ports(jack_client_t *c, const char *a,
      const char *b, unsigned long flags)
{
   const char **list = (const char**)malloc(3 * sizeof(*list));
   (void)c; (void)a; (void)b; (void)flags;
   if (!list)
      return NULL;
   list[0] = "system:playback_1";
   list[1] = "system:playback_2";
   list[2] = NULL;
   return list;
}

static void fake_on_shutdown(jack_client_t *c, JackShutdownCallback cb, void *arg)
{ (void)c; (void)cb; (void)arg; }

static void *fake_port_get_buffer(jack_port_t *port, jack_nframes_t n)
{
   (void)n;
   return port->buf;
}

static void fake_port_get_latency_range(jack_port_t *port,
      jack_latency_callback_mode_t mode, jack_latency_range_t *range)
{
   (void)port; (void)mode;
   range->min = range->max = 128;
}

static const char *fake_port_name(const jack_port_t *port) { return port->name; }

static jack_port_t *fake_port_register(jack_client_t *c, const char *name,
      const char *type, unsigned long flags, unsigned long bufsize)
{
   jack_port_t *p;
   (void)c; (void)type; (void)flags; (void)bufsize;
   if (fake_nports >= 8)
      return NULL;
   p = &fake_ports[fake_nports++];
   strncpy(p->name, name, sizeof(p->name) - 1);
   p->name[sizeof(p->name) - 1] = '\0';
   return p;
}

static int fake_set_buffer_size_callback(jack_client_t *c,
      JackBufferSizeCallback cb, void *arg)
{ (void)c; (void)cb; (void)arg; return 0; }

static int fake_set_process_callback(jack_client_t *c,
      JackProcessCallback cb, void *arg)
{
   (void)c;
   fake_process     = cb;
   fake_process_arg = arg;
   return 0;
}

static int fake_set_sample_rate_callback(jack_client_t *c,
      JackSampleRateCallback cb, void *arg)
{ (void)c; (void)cb; (void)arg; return 0; }

static int fake_set_xrun_callback(jack_client_t *c,
      JackXRunCallback cb, void *arg)
{ (void)c; (void)cb; (void)arg; return 0; }

static jack_ringbuffer_t *fake_rb_create(size_t sz)
{
   jack_ringbuffer_t *rb = (jack_ringbuffer_t*)calloc(1, sizeof(*rb));
   if (!rb)
      return NULL;
   if (!(rb->buf = (char*)malloc(sz)))
   {
      free(rb);
      return NULL;
   }
   rb->size = sz;
   n_rb_create++;
   return rb;
}

static void fake_rb_free(jack_ringbuffer_t *rb)
{
   n_rb_free++;
   free(rb->buf);
   free(rb);
}

static void fake_rb_get_read_vector(const jack_ringbuffer_t *rb,
      jack_ringbuffer_data_t *vec)
{
   size_t avail = rb->w - rb->r;
   size_t start = rb->r % rb->size;
   size_t first = rb->size - start;
   if (first > avail)
      first = avail;
   vec[0].buf = rb->buf + start;
   vec[0].len = first;
   vec[1].buf = rb->buf;
   vec[1].len = avail - first;
}

static int fake_rb_mlock(jack_ringbuffer_t *rb) { (void)rb; return 0; }

static void fake_rb_read_advance(jack_ringbuffer_t *rb, size_t cnt) { rb->r += cnt; }

static size_t fake_rb_write_space(const jack_ringbuffer_t *rb)
{
   return rb->size - (rb->w - rb->r);
}

static size_t fake_rb_write(jack_ringbuffer_t *rb, const char *src, size_t cnt)
{
   size_t i, space = fake_rb_write_space(rb);
   if (cnt > space)
      cnt = space;
   for (i = 0; i < cnt; i++)
      rb->buf[(rb->w + i) % rb->size] = src[i];
   rb->w += cnt;
   return cnt;
}

/* ---- the fake loader ------------------------------------------------ */

typedef struct
{
   const char *name;
   function_t  fn;
} fake_sym_t;

#define FAKE(name, fn) { name, (function_t)fn }

static const fake_sym_t fake_syms[] = {
   FAKE("jack_client_open",                fake_client_open),
   FAKE("jack_client_close",               fake_client_close),
   FAKE("jack_activate",                   fake_activate),
   FAKE("jack_deactivate",                 fake_deactivate),
   FAKE("jack_connect",                    fake_connect),
   FAKE("jack_free",                       fake_free),
   FAKE("jack_get_buffer_size",            fake_get_buffer_size),
   FAKE("jack_get_cycle_times",            fake_get_cycle_times),
   FAKE("jack_get_ports",                  fake_get_ports),
   FAKE("jack_get_sample_rate",            fake_get_sample_rate),
   FAKE("jack_on_shutdown",                fake_on_shutdown),
   FAKE("jack_port_get_buffer",            fake_port_get_buffer),
   FAKE("jack_port_name",                  fake_port_name),
   FAKE("jack_port_register",              fake_port_register),
   FAKE("jack_set_buffer_size_callback",   fake_set_buffer_size_callback),
   FAKE("jack_set_process_callback",       fake_set_process_callback),
   FAKE("jack_set_sample_rate_callback",   fake_set_sample_rate_callback),
   FAKE("jack_set_xrun_callback",          fake_set_xrun_callback),
   FAKE("jack_ringbuffer_create",          fake_rb_create),
   FAKE("jack_ringbuffer_free",            fake_rb_free),
   FAKE("jack_ringbuffer_get_read_vector", fake_rb_get_read_vector),
   FAKE("jack_ringbuffer_mlock",           fake_rb_mlock),
   FAKE("jack_ringbuffer_read_advance",    fake_rb_read_advance),
   FAKE("jack_ringbuffer_write",           fake_rb_write),
   FAKE("jack_ringbuffer_write_space",     fake_rb_write_space),
   /* Optional; listed last so the required ones are the leading run. */
   FAKE("jack_port_get_latency_range",     fake_port_get_latency_range)
};

#define FAKE_REQUIRED (sizeof(fake_syms) / sizeof(fake_syms[0]) - 1)

/* The names the driver tries, in order; only the first is checked. */
#if defined(__APPLE__)
#define JR_LIB_NAMES 2
#else
#define JR_LIB_NAMES 1
#endif

static int         lib_present;
static const char *lib_withheld;   /* a symbol the fake library lacks */
static int         lib_handle;
static int         n_load, n_proc, n_close_lib;
static char        lib_path[64];

dylib_t dylib_load(const char *path)
{
   if (!n_load++)
   {
      strncpy(lib_path, path, sizeof(lib_path) - 1);
      lib_path[sizeof(lib_path) - 1] = '\0';
   }
   return lib_present ? (dylib_t)&lib_handle : NULL;
}

function_t dylib_proc(dylib_t lib, const char *name)
{
   size_t i;
   n_proc++;
   if (lib != (dylib_t)&lib_handle)
      return NULL;
   if (lib_withheld && !strcmp(name, lib_withheld))
      return NULL;
   for (i = 0; i < sizeof(fake_syms) / sizeof(fake_syms[0]); i++)
      if (!strcmp(fake_syms[i].name, name))
         return fake_syms[i].fn;
   printf("[FAIL] the driver asked for an unknown symbol: %s\n", name);
   failures++;
   return NULL;
}

void dylib_close(dylib_t lib)
{
   (void)lib;
   n_close_lib++;
}

char *dylib_error(void) { return NULL; }

static void reset_counts(void)
{
   n_open = n_close = n_activate = n_deactivate = n_connect = n_free = 0;
   n_rb_create = n_rb_free = 0;
   n_load = n_proc = n_close_lib = 0;
   fake_process = NULL;
   fake_process_arg = NULL;
}

/* ---- scenarios ------------------------------------------------------ */

static void test_absent(void)
{
   unsigned rate = 0;
   void *sl;

   reset_counts();
   lib_present = 0;
   CHECK(audio_jack.init(NULL, 48000, 64, &rate) == NULL,
         "absent library: init must fail");
   sl = audio_jack.device_list_new(NULL);
   CHECK(sl == NULL, "absent library: device list must be empty");
   CHECK(n_load == 2 * JR_LIB_NAMES, "absent library: every name tried, per entry point");
   CHECK(n_open == 0, "absent library: nothing may be called");
#if defined(_WIN64)
   CHECK(!strcmp(lib_path, "libjack64.dll"), "library name");
#elif defined(_WIN32)
   CHECK(!strcmp(lib_path, "libjack.dll"), "library name");
#elif defined(__APPLE__)
   CHECK(!strcmp(lib_path, "libjack.0.dylib"), "library name tried first");
#else
   CHECK(!strcmp(lib_path, "libjack.so.0"), "library name");
#endif
   printf("[pass] absent library: driver unavailable, nothing called\n");
}

static void test_each_required_missing(void)
{
   size_t i;
   lib_present = 1;
   for (i = 0; i < FAKE_REQUIRED; i++)
   {
      unsigned rate = 0;
      reset_counts();
      lib_withheld = fake_syms[i].name;
      if (audio_jack.init(NULL, 48000, 64, &rate) != NULL)
      {
         printf("[FAIL] init came up without %s\n", lib_withheld);
         failures++;
      }
      if (n_open != 0 || n_close_lib != 1)
      {
         printf("[FAIL] without %s: open %d, handle closes %d\n",
               lib_withheld, n_open, n_close_lib);
         failures++;
      }
   }
   lib_withheld = NULL;
   printf("[pass] each of %u required entry points withheld: load refused\n",
         (unsigned)FAKE_REQUIRED);
}

static void test_running(void)
{
   unsigned rate = 0, i;
   float frames[256 * 2];
   void *jd;
   struct string_list *sl;
   int ok;

   /* The table is published by the first load that succeeds; withhold
    * the optional query so the driver runs without it from here on. */
   lib_present  = 1;
   lib_withheld = "jack_port_get_latency_range";

   reset_counts();
   fake_open_fails = 1;
   CHECK(audio_jack.init(NULL, 48000, 64, &rate) == NULL,
         "server not running: init must fail");
   CHECK(n_load == 1 && n_open == 1 && n_close_lib == 0,
         "library loaded once, kept, server asked once");

   reset_counts();
   fake_open_fails   = 0;
   jr_device_latency = (size_t)-1;
   jd = audio_jack.init(NULL, 48000, 64, &rate);
   CHECK(jd != NULL, "init must succeed against the fake server");
   if (!jd)
      return;
   CHECK(n_load == 0, "the published table is reused, not reloaded");
   CHECK(rate == 48000, "rate from jack_get_sample_rate");
   CHECK(n_activate == 1 && n_deactivate == 0, "activated, not deactivated");
   CHECK(n_connect == 2, "both physical ports connected");
   CHECK(n_free == 1, "port list released with jack_free");
   CHECK(n_rb_create == 1, "ring from jack_ringbuffer_create");
   CHECK(jr_device_latency == 0, "no latency query: no graph latency");
   CHECK(fake_process != NULL, "process callback registered");

   for (i = 0; i < 256; i++)
   {
      frames[i * 2]     = (float)i / 256.0f;
      frames[i * 2 + 1] = -(float)i / 256.0f;
   }
   CHECK(audio_jack.write(jd, frames, sizeof(frames)) == (ssize_t)sizeof(frames),
         "write accepted in full");
   if (fake_process)
      fake_process(256, fake_process_arg);
   ok = 1;
   for (i = 0; i < 256; i++)
      if (     fake_ports[0].buf[i] != frames[i * 2]
            || fake_ports[1].buf[i] != frames[i * 2 + 1])
         ok = 0;
   CHECK(ok, "written frames reach the port buffers deinterleaved");
   CHECK(audio_jack.underruns(jd) == 0, "no underrun on a full period");

   audio_jack.free(jd);
   CHECK(n_deactivate == 1 && n_close == 1, "deactivated and closed once");
   CHECK(deactivated_before_close, "deactivated before closed");
   CHECK(n_rb_free == 1, "ring released with jack_ringbuffer_free");
   CHECK(n_close_lib == 0, "library stays loaded");

   reset_counts();
   sl = (struct string_list*)audio_jack.device_list_new(NULL);
   CHECK(sl && sl->size == 2, "device list names both physical inputs");
   CHECK(n_open == 1 && n_close == 1 && n_free == 1,
         "device list client opened, port list freed, client closed");
   if (sl)
      audio_jack.device_list_free(NULL, sl);
   lib_withheld = NULL;
   printf("[pass] running without the latency query: plays, tears down in order\n");
}

int main(void)
{
   test_absent();
   test_each_required_missing();
   test_running();
   if (failures)
   {
      printf("jack_runtime_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("jack_runtime_test: all passed\n");
   return 0;
}

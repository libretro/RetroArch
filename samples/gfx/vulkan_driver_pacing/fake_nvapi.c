/* A stand-in nvapi64.dll for the Windows run: nvapi_QueryInterface
 * answers the four Reflex entry points gfx/common/d3d_driver_pacing.c
 * resolves, and every call is logged for the test to read back
 * through fake_nvapi_log(). */
#include <stdint.h>
#include <string.h>
#include <windows.h>

#define FAKE_NVAPI_MAX 256

#pragma pack(push, 8)
typedef struct
{
   uint32_t version;
   uint8_t  low_latency_mode;
   uint8_t  low_latency_boost;
   uint32_t minimum_interval_us;
   uint8_t  use_markers_to_optimize;
   uint8_t  use_min_queue_time;
   uint8_t  rsvd[30];
} sleep_mode_params_t;

typedef struct
{
   uint32_t version;
   uint64_t frame_id;
   uint32_t marker_type;
   uint64_t rsvd0;
   uint8_t  rsvd[56];
} marker_params_t;
#pragma pack(pop)

/* kind 0 the mode (a = mode | boost << 1, b = interval), 1 a sleep,
 * 2 a marker (a = frame, b = marker) */
typedef struct
{
   int kind;
   void *device;
   uint64_t a;
   unsigned b;
} fake_nvapi_event_t;

static fake_nvapi_event_t events[FAKE_NVAPI_MAX];
static unsigned num_events;
static int sleep_fails;

static void ev(int kind, void *device, uint64_t a, unsigned b)
{
   if (num_events < FAKE_NVAPI_MAX)
   {
      events[num_events].kind   = kind;
      events[num_events].device = device;
      events[num_events].a      = a;
      events[num_events].b      = b;
   }
   num_events++;
}

static int __cdecl fake_initialize(void) { return 0; }

static int __cdecl fake_set_sleep_mode(void *device,
      sleep_mode_params_t *p)
{
   if (!device || !p || (p->version >> 16) != 1
         || (p->version & 0xffff) != sizeof(*p))
      return -5;
   ev(0, device, p->low_latency_mode | (p->low_latency_boost << 1),
         p->minimum_interval_us);
   return 0;
}

static int __cdecl fake_sleep(void *device)
{
   if (!device)
      return -5;
   ev(1, device, 0, 0);
   return sleep_fails ? -1 : 0;
}

static int __cdecl fake_set_latency_marker(void *device, marker_params_t *p)
{
   if (!device || !p || (p->version >> 16) != 1
         || (p->version & 0xffff) != sizeof(*p))
      return -5;
   ev(2, device, p->frame_id, p->marker_type);
   return 0;
}

__declspec(dllexport) void *__cdecl nvapi_QueryInterface(unsigned int id)
{
   switch (id)
   {
      case 0x0150e828: return (void*)fake_initialize;
      case 0xac1ca9e0: return (void*)fake_set_sleep_mode;
      case 0x852cd1d2: return (void*)fake_sleep;
      case 0xd9984c05: return (void*)fake_set_latency_marker;
   }
   return NULL;
}

/* What was called since the last reset */
__declspec(dllexport) unsigned fake_nvapi_log(const fake_nvapi_event_t **out,
      int reset)
{
   unsigned n = num_events;
   *out        = events;
   if (reset)
      num_events = 0;
   return n;
}

/* Sleeps from now on fail, or succeed again */
__declspec(dllexport) void fake_nvapi_fail_sleep(int fail)
{
   sleep_fails = fail;
}

/* The D3DKMT binding and vblank clock of gfx/common/win32_common.c,
 * on entry points of the test's own: Wine has neither
 * D3DKMTGetScanLine nor D3DKMTWaitForVerticalBlankEvent, and a CI
 * machine has no display to wait on. The file is included for its
 * statics. */
#include "../../../gfx/common/win32_common.c"

#include <stdio.h>

static unsigned failures;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         failures++; \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
      } \
   } while (0)

enum { FK_TICK = 0, FK_FAIL, FK_INSTANT, FK_BLOCK };

static volatile LONG fk_opens;
static volatile LONG fk_open_fails;
static volatile LONG fk_closes;
static volatile LONG fk_closed;
static volatile LONG fk_waits;
static volatile LONG fk_wait_adapter;
static volatile LONG fk_scan_adapter;
static volatile LONG fk_mode;
static volatile LONG fk_hz = 60;

static NTSTATUS CALLBACK fk_open(D3DKMT_OPENADAPTERFROMHDC *d)
{
   LONG n = InterlockedIncrement(&fk_opens);
   if (fk_open_fails)
      return 1;
   d->hAdapter      = 0x100 + n;
   d->VidPnSourceId = 3;
   return STATUS_SUCCESS;
}

static NTSTATUS CALLBACK fk_close(const D3DKMT_CLOSEADAPTER *d)
{
   InterlockedIncrement(&fk_closes);
   InterlockedExchange(&fk_closed, (LONG)d->hAdapter);
   return STATUS_SUCCESS;
}

static NTSTATUS CALLBACK fk_scan(D3DKMT_GETSCANLINE *d)
{
   InterlockedExchange(&fk_scan_adapter, (LONG)d->hAdapter);
   d->InVerticalBlank = FALSE;
   d->ScanLine        = 100 + d->VidPnSourceId;
   return STATUS_SUCCESS;
}

/* A display at fk_hz, one that fails, one whose wait comes straight
 * back, and one that has stopped scanning out */
static NTSTATUS CALLBACK fk_wait(D3DKMT_WAITFORVERTICALBLANKEVENT *d)
{
   InterlockedIncrement(&fk_waits);
   InterlockedExchange(&fk_wait_adapter, (LONG)d->hAdapter);
   switch (fk_mode)
   {
      case FK_FAIL:
         return 1;
      case FK_INSTANT:
         return STATUS_SUCCESS;
      case FK_BLOCK:
         while (fk_mode == FK_BLOCK)
            Sleep(5);
         return 1;
      default:
         break;
   }
   {
      retro_time_t period = 1000000 / fk_hz;
      retro_time_t now    = cpu_features_get_time_usec();
      retro_time_t next   = (now / period + 1) * period;
      while (now < next)
      {
         if (next - now > 2500)
            Sleep(1);
         now = cpu_features_get_time_usec();
      }
   }
   return STATUS_SUCCESS;
}

/* Asks until the clock answers @want_ticking, a second at most */
static unsigned ask_until(bool want_ticking)
{
   unsigned i;
   unsigned n      = 0;
   retro_time_t at = 0;
   for (i = 0; i < 200; i++)
   {
      n = d3dkmt_vblank_wait(0, 0, &at);
      if ((n != 0) == want_ticking)
         break;
      Sleep(5);
   }
   return n;
}

int main(void)
{
   unsigned i, n, seen, gen;
   LONG calls;
   retro_time_t at = 0;
   retro_time_t t0, took;
   D3DKMT_HANDLE adapter;
   UINT source;
   HWND hwnd;

   /* The system's own entry points first: Windows before Vista and
    * Wine lack some, and nothing may be called through a missing one */
   d3dkmt_init();
   if (!pD3DKMTGetScanLine)
      CHECK(d3dkmt_scanline_get() == -1, "a scanline with no entry point");
   if (!pD3DKMTWaitForVerticalBlankEvent)
      CHECK(!d3dkmt_vblank_wait(0, 0, &at), "vblanks with no entry point");

   hwnd = CreateWindowA("STATIC", "d3dkmt", WS_OVERLAPPEDWINDOW,
         10, 10, 200, 200, NULL, NULL, GetModuleHandle(NULL), NULL);
   CHECK(hwnd != NULL, "no window");

   pD3DKMTOpenAdapterFromHdc        = fk_open;
   pD3DKMTCloseAdapter              = fk_close;
   pD3DKMTGetScanLine               = fk_scan;
   pD3DKMTWaitForVerticalBlankEvent = fk_wait;
   d3dkmt_bind(0, 0);
   d3dkmt_monitor = NULL;
   fk_closes      = 0;

   /* ---- the binding follows the window ---- */
   CHECK(!d3dkmt_bound(&adapter, &source), "bound before a window said where");
   CHECK(d3dkmt_scanline_get() == -1, "a scanline with nothing bound");
   gen = win32_d3dkmt_generation();
   win32_d3dkmt_follow(hwnd, false);
   CHECK(win32_d3dkmt_generation() == gen + 1,
         "a window on its first monitor is not another output");
   CHECK(fk_opens == 1 && d3dkmt_bound(&adapter, &source)
         && adapter == 0x101 && source == 3,
         "window's monitor not bound: %ld opens, adapter %x source %u",
         (long)fk_opens, (unsigned)adapter, (unsigned)source);
   CHECK(d3dkmt_scanline_get() == 103 && fk_scan_adapter == 0x101,
         "scanline not read from the bound source");
   /* a window dragged about its monitor asks on every move */
   for (i = 0; i < 100; i++)
      win32_d3dkmt_follow(hwnd, false);
   CHECK(fk_opens == 1 && !fk_closes, "same monitor rebound: %ld opens",
         (long)fk_opens);
   CHECK(win32_d3dkmt_generation() == gen + 1,
         "moves about one monitor counted as other outputs");
   /* a display change rebinds, and closes what it leaves */
   win32_d3dkmt_follow(hwnd, true);
   CHECK(win32_d3dkmt_generation() == gen + 2,
         "a display change is not another output");
   CHECK(fk_opens == 2 && fk_closes == 1 && fk_closed == 0x101
         && d3dkmt_bound(&adapter, &source) && adapter == 0x102,
         "display change: %ld opens, %ld closes of %lx, adapter %x",
         (long)fk_opens, (long)fk_closes, (long)fk_closed, (unsigned)adapter);
   win32_d3dkmt_follow(NULL, true);
   CHECK(fk_opens == 2, "followed no window");
   CHECK(win32_d3dkmt_generation() == gen + 2, "no window counted");
   /* a monitor that does not open leaves the last binding */
   fk_open_fails = 1;
   win32_d3dkmt_follow(hwnd, true);
   fk_open_fails = 0;
   /* but is another output all the same, as it is with no D3DKMT */
   CHECK(win32_d3dkmt_generation() == gen + 3,
         "an output that does not open is not counted");
   pD3DKMTOpenAdapterFromHdc = NULL;
   win32_d3dkmt_follow(hwnd, true);
   pD3DKMTOpenAdapterFromHdc = fk_open;
   CHECK(win32_d3dkmt_generation() == gen + 4 && fk_opens == 3,
         "an output without D3DKMT is not counted");
   CHECK(fk_opens == 3 && fk_closes == 1
         && d3dkmt_bound(&adapter, &source) && adapter == 0x102,
         "failed open: %ld closes, adapter %x", (long)fk_closes,
         (unsigned)adapter);

   /* ---- the clock counts the display's vblanks ---- */
   seen = ask_until(true);
   CHECK(seen, "the vblank clock never started");
   t0   = cpu_features_get_time_usec();
   for (i = 0; i < 30; i++)
   {
      retro_time_t now;
      n   = d3dkmt_vblank_wait(seen, 200000, &at);
      now = cpu_features_get_time_usec();
      CHECK(n && n != seen, "vblank %u: count %u after %u", i, n, seen);
      CHECK(at <= now && now - at < 100000,
            "vblank %u: timed %ld us before now", i, (long)(now - at));
      seen = n;
   }
   took = cpu_features_get_time_usec() - t0;
   CHECK(took > 350000 && took < 800000,
         "30 vblanks at 60 Hz in %ld us", (long)took);
   CHECK(fk_wait_adapter == 0x102, "waited on adapter %lx",
         (long)fk_wait_adapter);

   /* ---- and follows a rebind ---- */
   win32_d3dkmt_follow(hwnd, true);
   for (i = 0; i < 10 && fk_wait_adapter != 0x104; i++)
      seen = d3dkmt_vblank_wait(seen, 200000, &at);
   CHECK(fk_wait_adapter == 0x104 && fk_closed == 0x102,
         "after a rebind: waits on %lx, closed %lx",
         (long)fk_wait_adapter, (long)fk_closed);
   seen = ask_until(true);

   /* ---- a display that stops: the caller's wait is bounded ---- */
   InterlockedExchange(&fk_mode, FK_BLOCK);
   took = 0;
   for (i = 0; i < 3; i++)
   {
      t0   = cpu_features_get_time_usec();
      n    = d3dkmt_vblank_wait(seen, 50000, &at);
      took = cpu_features_get_time_usec() - t0;
      if (n == seen)
         break;
      seen = n;
   }
   CHECK(n == seen && took >= 40000 && took < 500000,
         "stopped display: count %u after %u, in %ld us", n, seen,
         (long)took);

   /* ---- a wait that fails: none counted, and no retry unasked ---- */
   InterlockedExchange(&fk_mode, FK_FAIL);
   CHECK(!ask_until(false), "a failing wait still counts");
   Sleep(100);
   calls = fk_waits;
   Sleep(300);
   CHECK(fk_waits == calls, "a failing wait retried unasked: %ld times",
         (long)(fk_waits - calls));
   InterlockedExchange(&fk_mode, FK_TICK);
   CHECK(ask_until(true), "the clock did not come back");

   /* ---- a wait that comes straight back is not spun on ---- */
   InterlockedExchange(&fk_mode, FK_INSTANT);
   calls = fk_waits;
   for (i = 0; i < 30; i++)
   {
      d3dkmt_vblank_wait(0, 0, &at);
      Sleep(10);
   }
   CHECK(fk_waits - calls < 200, "spun on a wait that does not wait: %ld calls",
         (long)(fk_waits - calls));
   InterlockedExchange(&fk_mode, FK_TICK);

   /* ---- unasked, it parks after D3DKMT_VBLANK_IDLE vblanks ---- */
   InterlockedExchange(&fk_hz, 1000);
   CHECK(ask_until(true), "the clock did not come back at 1000 Hz");
   /* counted from before the last ask, a few vblanks ahead of the one
    * the clock takes it on */
   calls = fk_waits;
   d3dkmt_vblank_wait(0, 0, &at);
   for (i = 0; i < 100; i++)
   {
      LONG before = fk_waits;
      Sleep(200);
      if (fk_waits == before || fk_waits - calls > D3DKMT_VBLANK_IDLE + 64)
         break;
   }
   CHECK(fk_waits - calls >= D3DKMT_VBLANK_IDLE
         && fk_waits - calls <= D3DKMT_VBLANK_IDLE + 64,
         "unasked, the clock waited on %ld vblanks before parking",
         (long)(fk_waits - calls));
   calls = fk_waits;
   Sleep(200);
   CHECK(fk_waits == calls, "parked, the clock kept waiting: %ld vblanks",
         (long)(fk_waits - calls));
   CHECK(ask_until(true), "parked, the clock did not come back when asked");

   if (hwnd)
      DestroyWindow(hwnd);
   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("PASS win32_d3dkmt_test\n");
   return 0;
}

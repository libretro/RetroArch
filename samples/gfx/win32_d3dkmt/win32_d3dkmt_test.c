/* The D3DKMT binding of gfx/common/win32_common.c, on entry points of
 * the test's own: Wine has neither D3DKMTGetScanLine nor
 * D3DKMTWaitForVerticalBlankEvent, and a CI machine has one display.
 * The file is included for its statics. */
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

static volatile LONG fk_opens;
static volatile LONG fk_open_fails;
static volatile LONG fk_closes;
static volatile LONG fk_closed;
static volatile LONG fk_waits;
static volatile LONG fk_wait_adapter;
static volatile LONG fk_scan_adapter;

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

static NTSTATUS CALLBACK fk_wait(D3DKMT_WAITFORVERTICALBLANKEVENT *d)
{
   InterlockedIncrement(&fk_waits);
   InterlockedExchange(&fk_wait_adapter, (LONG)d->hAdapter);
   return STATUS_SUCCESS;
}

int main(void)
{
   unsigned i;
   D3DKMT_HANDLE adapter;
   UINT source;
   HWND hwnd;

   /* The system's own entry points first: Windows before Vista and
    * Wine lack some, and nothing may be called through a missing one */
   d3dkmt_init();
   if (!pD3DKMTGetScanLine)
      CHECK(d3dkmt_scanline_get() == -1, "a scanline with no entry point");
   if (!pD3DKMTWaitForVerticalBlankEvent)
      CHECK(!d3dkmt_wait_vblank(), "a vblank with no entry point");

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
   CHECK(!d3dkmt_wait_vblank() && !fk_waits, "a vblank with nothing bound");
   win32_d3dkmt_follow(hwnd, false);
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
   /* a display change rebinds, and closes what it leaves */
   win32_d3dkmt_follow(hwnd, true);
   CHECK(fk_opens == 2 && fk_closes == 1 && fk_closed == 0x101
         && d3dkmt_bound(&adapter, &source) && adapter == 0x102,
         "display change: %ld opens, %ld closes of %lx, adapter %x",
         (long)fk_opens, (long)fk_closes, (long)fk_closed, (unsigned)adapter);
   win32_d3dkmt_follow(NULL, true);
   CHECK(fk_opens == 2, "followed no window");
   /* a monitor that does not open leaves the last binding */
   fk_open_fails = 1;
   win32_d3dkmt_follow(hwnd, true);
   fk_open_fails = 0;
   CHECK(fk_opens == 3 && fk_closes == 1
         && d3dkmt_bound(&adapter, &source) && adapter == 0x102,
         "failed open: %ld closes, adapter %x", (long)fk_closes,
         (unsigned)adapter);

   CHECK(d3dkmt_wait_vblank() && fk_waits == 1 && fk_wait_adapter == 0x102,
         "vblank not waited for on the bound source: adapter %lx",
         (long)fk_wait_adapter);

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

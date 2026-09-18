/* Scanline Sync's XDDM tier (Windows 2000 and XP) against a real
 * DirectDraw: IDirectDraw::GetScanLine readings, taken at varying points
 * of each frame as the frame loop takes them, through
 * vblank_sampler_feed() into vblank_clock - the same two calls
 * gfx/common/win32_common.c makes - and the clock's beam then checked
 * against fresh GetScanLine reads.
 *
 * Built with MinGW; run under wine, whose DirectDraw emulates the
 * counter the way an XDDM driver reports it (lines in the active region,
 * DDERR_VERTICALBLANKINPROGRESS in blanking), or on Windows.
 *
 *   the clock settles from readings alone
 *   its beam matches GetScanLine within 10 lines */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#include "gfx/common/vblank_clock.h"

static int64_t now_us(void)
{
   static LARGE_INTEGER f; LARGE_INTEGER c;
   if (!f.QuadPart) QueryPerformanceFrequency(&f);
   QueryPerformanceCounter(&c);
   return (int64_t)(c.QuadPart / f.QuadPart * 1000000 + c.QuadPart % f.QuadPart * 1000000 / f.QuadPart);
}

int main(void)
{
   typedef HRESULT (WINAPI *create_fn)(GUID*, LPDIRECTDRAW*, IUnknown*);
   HMODULE dll = LoadLibraryA("ddraw.dll");
   create_fn create = dll ? (create_fn)GetProcAddress(dll, "DirectDrawCreate") : NULL;
   LPDIRECTDRAW dd = NULL;
   DEVMODEA dm;
   vblank_clock_t vc;
   vblank_sampler_t smp;
   unsigned active, total = 0;
   double nominal;
   int i, frame, errs = 0, worst = 0, checks = 0, blanks = 0, lines = 0;
   DWORD maxline = 0;

   memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
   EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dm);
   active  = dm.dmPelsHeight;
   nominal = 1000000.0 / (dm.dmDisplayFrequency > 1 ? dm.dmDisplayFrequency : 60);
   printf("mode %lux%lu @ %lu Hz\n", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency);
   if (!create || FAILED(create(NULL, &dd, NULL))) { printf("no DirectDraw\n"); return 2; }

   /* what wine's counter looks like */
   for (i = 0; i < 20000; i++)
   {
      DWORD l = 0; HRESULT hr = dd->lpVtbl->GetScanLine(dd, &l);
      if (hr == DD_OK) { lines++; if (l > maxline) maxline = l; }
      else if (hr == DDERR_VERTICALBLANKINPROGRESS) blanks++;
      else errs++;
   }
   printf("counter: %d lines (max %lu), %d in-blank, %d errors\n", lines, maxline, blanks, errs);

   memset(&smp, 0, sizeof(smp));
   vblank_clock_init(&vc, nominal);
   for (frame = 0; frame < 400; frame++)
   {
      int r;
      for (r = 0; r < 2; r++)
      {
         DWORD l = 0; int64_t t0, t1, vb; HRESULT hr; unsigned tot = 0;
         /* readings at varying points in the frame, as a frame loop
          * would take them - a Sleep stands in for the core running */
         Sleep(1 + (frame * 7 + r * 5) % 6);
         t0 = now_us(); hr = dd->lpVtbl->GetScanLine(dd, &l); t1 = now_us();
         vb = vblank_sampler_feed(&smp, t0 + (t1 - t0) / 2,
               hr == DD_OK ? (int)l : -1, active,
               vc.good >= VBLANK_CLOCK_SETTLE ? vc.period_us : nominal, &tot);
         if (vb) { total = tot; vblank_clock_feed(&vc, vb); }
      }
      if (frame > 200)
      {
         DWORD l = 0; int64_t t0 = now_us();
         HRESULT hr = dd->lpVtbl->GetScanLine(dd, &l);
         int64_t t1 = now_us();
         int beam = total ? vblank_clock_beam(&vc, t0 + (t1 - t0) / 2, active, total) : -1;
         if (hr == DD_OK && beam >= 0 && beam < (int)active)
         {
            int d = abs((int)l - beam);
            if (d > (int)total / 2) d = (int)total - d;
            if (d > worst) worst = d;
            checks++;
         }
      }
   }
   printf("sampler: rate %.5f lines/us from %u pairs, total lines %u\n", smp.rate, smp.rate_n, total);
   printf("clock: settled %s, period %.1f us (nominal %.1f)\n", vc.good >= VBLANK_CLOCK_SETTLE ? "yes" : "no", vc.period_us, nominal);
   printf("beam against GetScanLine: worst %d lines over %d checks\n", worst, checks);
   dd->lpVtbl->Release(dd);
   return (vc.good >= VBLANK_CLOCK_SETTLE && checks > 100 && worst <= 10) ? 0 : 1;
}

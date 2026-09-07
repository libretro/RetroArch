/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_win32_vendor_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The three Windows custom-timing paths against stand-ins for the
 * hardware they program.
 *
 * ADL: the backend loads the mock atiadlxx.dll next to the exe, maps
 * \\.\DISPLAY1 to the mock's adapter and display, stages an add and
 * an update, and flush issues one forced Set - the mock re-plugs the
 * monitor in response and the resync helper's arm/wait pair must see
 * it rather than time out - after which the table the mock holds
 * carries exactly the generated timing, polarities inverted the way
 * the driver version rule says. get_timing reads it back through the
 * override query and the cached list.
 *
 * ATI legacy: a scratch key under HKLM stands in for the adapter's
 * registry key. update writes DALDTMCRTBCD<w>x<h>x0x<r>; the 68 bytes
 * are checked field by field against the BCD encoding the driver
 * reads, and get_timing decodes them back to the same modeline.
 *
 * PowerStrip: a window of class TPShidden in this process answers
 * UM_GETTIMING and UM_SETCUSTOMTIMING through global atoms. The
 * backend must find it, back up the timing on open, send the
 * generated timing as a PowerStrip string on update, and put the
 * backup back on close. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../../gfx/display_servers/win32_modeline.h"
#include "../../../gfx/modeline/modeline_core.h"
#include "../../../gfx/modeline/modeline_monitor.h"
#include "mock_adl.h"

static int s_verbose;

void live_log(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

#define STUB_LOG(name, always) \
   void name(const char *fmt, ...) \
   { \
      va_list ap; \
      if (!(always) && !s_verbose) \
         return; \
      va_start(ap, fmt); \
      vfprintf(stderr, fmt, ap); \
      va_end(ap); \
   }
STUB_LOG(RARCH_DBG, 0)
STUB_LOG(RARCH_LOG, 0)
STUB_LOG(RARCH_WARN, 0)
STUB_LOG(RARCH_ERR, 1)

/* One generated 15 kHz timing, the same for all three paths */
static void make_timing(video_modeline_t *m)
{
   video_modeline_range_t range[MODELINE_MAX_RANGES];
   video_modeline_gen_t gen;
   video_modeline_t s;

   memset(range, 0, sizeof(range));
   memset(&gen, 0, sizeof(gen));
   memset(&s, 0, sizeof(s));
   memset(m, 0, sizeof(*m));
   modeline_monitor_set_preset("arcade_15", range);
   gen.interlace          = 1;
   gen.doublescan         = 1;
   gen.monitor_aspect     = MODELINE_STANDARD_CRT_ASPECT;
   gen.refresh_tolerance  = 2.0;
   gen.super_width        = 2560;
   gen.h_size             = 1.0;
   gen.pixel_precision    = 1;
   gen.scale_proportional = 1;

   s.hactive = 320;
   s.vactive = 240;
   s.vfreq   = 60.0;
   m->type   = MODELINE_XYV_EDITABLE | MODELINE_SCAN_EDITABLE;
   m->hactive = 320;
   m->vactive = 240;
   m->vfreq   = 60.0;
   modeline_create(&s, m, &range[0], &gen);
   m->width   = m->hactive;
   m->height  = m->vactive;
   m->refresh = (int)m->vfreq;
}

static int same_timing(const video_modeline_t *a, const video_modeline_t *b,
      const char *what)
{
   if (a->hactive != b->hactive || a->hbegin != b->hbegin || a->hend != b->hend
         || a->htotal != b->htotal || a->vactive != b->vactive
         || a->vbegin != b->vbegin || a->vend != b->vend || a->vtotal != b->vtotal
         || a->interlace != b->interlace || a->hsync != b->hsync || a->vsync != b->vsync
         || a->pclock / 10000 != b->pclock / 10000)
   {
      fprintf(stderr, "FAIL: %s: %d %d %d %d %d %d %d %d i%d h%d v%d %llu vs %d %d %d %d %d %d %d %d i%d h%d v%d %llu\n",
            what, a->hactive, a->hbegin, a->hend, a->htotal, a->vactive, a->vbegin,
            a->vend, a->vtotal, a->interlace, a->hsync, a->vsync,
            (unsigned long long)a->pclock,
            b->hactive, b->hbegin, b->hend, b->htotal, b->vactive, b->vbegin,
            b->vend, b->vtotal, b->interlace, b->hsync, b->vsync,
            (unsigned long long)b->pclock);
      return 1;
   }
   return 0;
}

/* ------------------------------------------------------------------ ADL */

typedef int (*mock_adl_state_t)(int*, int*, int*, ADLDisplayModeInfo*, int);

static int test_adl(void)
{
   win32_modeline_backend_t b;
   video_modeline_disp_t ds;
   video_modeline_t m, listed, back;
   HMODULE mock;
   mock_adl_state_t state;
   ADLDisplayModeInfo table[4];
   int n, sets, force;
   DWORD t0, t1;

   memset(&b, 0, sizeof(b));
   memset(&ds, 0, sizeof(ds));
   if (!win32_modeline_adl_create(&b, "\\\\.\\DISPLAY1", "Software\\RetroArchTest\\adl", &ds))
   {
      fprintf(stderr, "FAIL: ADL create declined with the mock dll present\n");
      return 1;
   }
   /* Unpatched, unknown driver version: update only */
   if (b.caps(b.ctx) != 0)
   {
      fprintf(stderr, "FAIL: ADL caps %u without allow_hardware_refresh or a patched driver\n",
            b.caps(b.ctx));
      return 1;
   }
   printf("[pass] ADL: mock driver loaded, adapter and display mapped\n");

   mock  = GetModuleHandleA("atiadlxx.dll");
   state = mock ? (mock_adl_state_t)GetProcAddress(mock, "mock_adl_state") : NULL;
   if (!state)
   {
      fprintf(stderr, "FAIL: mock dll not the one loaded\n");
      return 1;
   }

   /* A listed 320x240@60 system mode gets the generated timing */
   make_timing(&m);
   memset(&listed, 0, sizeof(listed));
   listed.width   = listed.hactive = 320;
   listed.height  = listed.vactive = 240;
   listed.refresh = 60;
   listed.vfreq   = 60;
   if (b.get_timing(b.ctx, &listed))
   {
      fprintf(stderr, "FAIL: get_timing found an override before any was written\n");
      return 1;
   }

   if (!b.update_mode(b.ctx, &m))
   {
      fprintf(stderr, "FAIL: ADL update_mode\n");
      return 1;
   }
   state(&n, &sets, &force, table, 4);
   if (n != 1 || force != 0)
   {
      fprintf(stderr, "FAIL: after staging, %d override(s), %d forced set(s)\n", n, force);
      return 1;
   }
   t0 = GetTickCount();
   if (!b.flush(b.ctx))
   {
      fprintf(stderr, "FAIL: ADL flush\n");
      return 1;
   }
   t1 = GetTickCount();
   state(&n, &sets, &force, table, 4);
   if (force != 1)
   {
      fprintf(stderr, "FAIL: flush issued %d forced set(s), expected 1\n", force);
      return 1;
   }
   /* The mock re-plugs synchronously; the wait must have returned on
    * the notification, well inside the 5 s bound */
   if (t1 - t0 > 2000)
   {
      fprintf(stderr, "FAIL: flush took %lu ms: the resync wait timed out instead of seeing the re-plug\n",
            (unsigned long)(t1 - t0));
      return 1;
   }
   printf("[pass] ADL: staged update, one forced set at flush, resync saw the re-plug in %lu ms\n",
         (unsigned long)(t1 - t0));

   /* What the driver holds is the generated timing */
   if (table[0].iPelsWidth != m.width || table[0].iPelsHeight != m.height
         || table[0].iRefreshRate != m.refresh
         || table[0].sDetailedTiming.sHTotal != m.htotal
         || table[0].sDetailedTiming.sHDisplay != m.hactive
         || table[0].sDetailedTiming.sHSyncStart != m.hbegin
         || table[0].sDetailedTiming.sHSyncWidth != m.hend - m.hbegin
         || table[0].sDetailedTiming.sVTotal != m.vtotal
         || table[0].sDetailedTiming.sVDisplay != m.vactive
         || table[0].sDetailedTiming.sVSyncStart != m.vbegin
         || table[0].sDetailedTiming.sVSyncWidth != m.vend - m.vbegin
         || table[0].sDetailedTiming.sPixelClock != (unsigned short)(m.pclock / 10000)
         || table[0].iTimingStandard != ADL_DL_MODETIMING_STANDARD_CUSTOM)
   {
      fprintf(stderr, "FAIL: the override table does not carry the generated timing\n");
      return 1;
   }
   /* Unknown driver version counts as <= 12: polarity bits inverted on write */
   if (!!(table[0].sDetailedTiming.sTimingFlags & ADL_DL_TIMINGFLAG_H_SYNC_POLARITY) != !m.hsync
         || !!(table[0].sDetailedTiming.sTimingFlags & ADL_DL_TIMINGFLAG_V_SYNC_POLARITY) != !m.vsync)
   {
      fprintf(stderr, "FAIL: polarity bits not inverted for a <=12 driver\n");
      return 1;
   }
   printf("[pass] ADL: override table holds the generated timing with driver polarity\n");

   /* Read it back: the query path, and the cache the list refresh rebuilt */
   back = listed;
   if (!b.get_timing(b.ctx, &back) || !(back.type & MODELINE_TIMING_ATI_ADL))
   {
      fprintf(stderr, "FAIL: get_timing did not find the written override\n");
      return 1;
   }
   if (same_timing(&back, &m, "ADL read-back"))
      return 1;
   printf("[pass] ADL: get_timing reads the override back as the same modeline\n");

   /* Delete stages a driver-default entry; flush forces it */
   if (!b.delete_mode(b.ctx, &m) || !b.flush(b.ctx))
   {
      fprintf(stderr, "FAIL: ADL delete + flush\n");
      return 1;
   }
   state(&n, &sets, &force, table, 4);
   if (n != 0 || force != 2)
   {
      fprintf(stderr, "FAIL: after delete, %d override(s), %d forced set(s)\n", n, force);
      return 1;
   }
   printf("[pass] ADL: delete removed the override through a forced set\n");

   b.close(b.ctx);
   return 0;
}

/* ------------------------------------------------------------ ATI legacy */

#define ATI_KEY "Software\\RetroArchTest\\ati"

static unsigned bcd_read(const BYTE *p)
{
   /* Four bytes, each two decimal digits */
   return ((p[0] >> 4) * 10 + (p[0] & 15)) * 1000000u
      +  ((p[1] >> 4) * 10 + (p[1] & 15)) * 10000u
      +  ((p[2] >> 4) * 10 + (p[2] & 15)) * 100u
      +  ((p[3] >> 4) * 10 + (p[3] & 15));
}

static unsigned be_read(const BYTE *p)
{
   return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static int test_ati(void)
{
   win32_modeline_backend_t b;
   video_modeline_disp_t ds;
   video_modeline_t m, back;
   HKEY key;
   BYTE data[68];
   DWORD len = sizeof(data);
   char name[64];
   unsigned checksum;

   RegDeleteKeyA(HKEY_LOCAL_MACHINE, ATI_KEY);
   if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, ATI_KEY, 0, NULL, 0, KEY_ALL_ACCESS,
            NULL, &key, NULL) != ERROR_SUCCESS)
   {
      puts("[skip] ATI legacy: cannot create a scratch HKLM key");
      return 0;
   }
   /* The driver only rewrites values that exist: seed the mode's entry */
   memset(data, 0, sizeof(data));
   make_timing(&m);
   snprintf(name, sizeof(name), "DALDTMCRTBCD%dx%dx0x%d", m.width, m.height, m.refresh);
   RegSetValueExA(key, name, 0, REG_BINARY, data, sizeof(data));
   RegCloseKey(key);

   memset(&b, 0, sizeof(b));
   memset(&ds, 0, sizeof(ds));
   if (!win32_modeline_ati_create(&b, "\\\\.\\DISPLAY1", ATI_KEY, &ds))
   {
      puts("[skip] ATI legacy: create declined (needs an elevated process)");
      return 0;
   }
   if (b.caps(b.ctx) != (MODELINE_CAPS_UPDATE | MODELINE_CAPS_SCAN_EDITABLE))
   {
      fprintf(stderr, "FAIL: ATI caps %u\n", b.caps(b.ctx));
      return 1;
   }

   if (!b.update_mode(b.ctx, &m) || !b.flush(b.ctx))
   {
      fprintf(stderr, "FAIL: ATI update_mode + flush\n");
      return 1;
   }

   if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, ATI_KEY, 0, KEY_READ, &key) != ERROR_SUCCESS
         || RegQueryValueExA(key, name, NULL, NULL, data, &len) != ERROR_SUCCESS
         || len != 68)
   {
      fprintf(stderr, "FAIL: ATI: %s not written\n", name);
      return 1;
   }
   RegCloseKey(key);

   /* The DALDTMCRTBCD layout: flags at 0, then htotal 4, hactive 8,
    * hbegin 12, hsync width 16, vtotal 20, vactive 24, vbegin 28,
    * vsync width 32, pclock/10k 36, checksum 64, all BCD except the
    * flags and the checksum */
   if (bcd_read(data + 4) != (unsigned)m.htotal || bcd_read(data + 8) != (unsigned)m.hactive
         || bcd_read(data + 12) != (unsigned)m.hbegin
         || bcd_read(data + 16) != (unsigned)(m.hend - m.hbegin)
         || bcd_read(data + 20) != (unsigned)m.vtotal || bcd_read(data + 24) != (unsigned)m.vactive
         || bcd_read(data + 28) != (unsigned)m.vbegin
         || bcd_read(data + 32) != (unsigned)(m.vend - m.vbegin)
         || bcd_read(data + 36) != (unsigned)(m.pclock / 10000))
   {
      fprintf(stderr, "FAIL: ATI: registry BCD fields do not match the modeline\n");
      return 1;
   }
   /* Negative polarity is a set bit in the flags word */
   if (!!(be_read(data) & 0x0004) != !m.hsync || !!(be_read(data) & 0x0008) != !m.vsync
         || !!(be_read(data) & 0x0002) != !!m.interlace)
   {
      fprintf(stderr, "FAIL: ATI: flags word %08x for i%d h%d v%d\n", be_read(data),
            m.interlace, m.hsync, m.vsync);
      return 1;
   }
   checksum = 65535 - be_read(data) - m.htotal - m.hactive - m.hend - m.vtotal
      - m.vactive - m.vend - (unsigned)(m.pclock / 10000);
   if (be_read(data + 64) != checksum)
   {
      fprintf(stderr, "FAIL: ATI: checksum %08x, expected %08x\n", be_read(data + 64), checksum);
      return 1;
   }
   printf("[pass] ATI legacy: %s written in the driver's BCD layout with its checksum\n", name);

   memset(&back, 0, sizeof(back));
   back.width   = m.width;
   back.height  = m.height;
   back.refresh = m.refresh;
   if (!b.get_timing(b.ctx, &back) || !(back.type & MODELINE_TIMING_ATI_LEGACY))
   {
      fprintf(stderr, "FAIL: ATI get_timing did not read the value\n");
      return 1;
   }
   if (same_timing(&back, &m, "ATI read-back"))
      return 1;
   printf("[pass] ATI legacy: get_timing decodes it back to the same modeline\n");

   b.close(b.ctx);
   RegDeleteKeyA(HKEY_LOCAL_MACHINE, ATI_KEY);
   return 0;
}

/* ------------------------------------------------------------- PowerStrip */

#define UM_SETCUSTOMTIMING (WM_USER + 200)
#define UM_GETTIMING       (WM_USER + 205)

static char s_ps_timing[256] = "1280,110,40,220,720,5,5,20,74250,0";
static int  s_ps_sets;

static LRESULT CALLBACK ps_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   switch (msg)
   {
      case UM_GETTIMING:
         return (LRESULT)GlobalAddAtomA(s_ps_timing);
      case UM_SETCUSTOMTIMING:
         {
            char in[256];
            int a, b, c, d, e, f, g, h, pclk, fl;
            if (!GlobalGetAtomNameA((ATOM)lparam, in, sizeof(in)))
               return -1;
            if (sscanf(in, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                     &a, &b, &c, &d, &e, &f, &g, &h, &pclk, &fl) != 10)
               return -1;
            strcpy(s_ps_timing, in);
            s_ps_sets++;
            return pclk * 1000;
         }
      default:
         break;
   }
   return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static int test_pstrip(void)
{
   WNDCLASSA wc;
   HWND hwnd;
   win32_modeline_backend_t b;
   video_modeline_disp_t ds;
   video_modeline_t m, listed;
   int a, hfp, hsw, hbp, e, vfp, vsw, vbp, pclk, fl;
   const char backup[] = "1280,110,40,220,720,5,5,20,74250,0";

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = ps_wnd_proc;
   wc.hInstance     = GetModuleHandleA(NULL);
   wc.lpszClassName = "TPShidden";
   RegisterClassA(&wc);
   hwnd = CreateWindowExA(0, "TPShidden", NULL, WS_POPUP, 0, 0, 1, 1,
         NULL, NULL, wc.hInstance, NULL);
   if (!hwnd)
   {
      fprintf(stderr, "FAIL: could not create the PowerStrip stand-in window\n");
      return 1;
   }
   strcpy(s_ps_timing, backup);

   memset(&b, 0, sizeof(b));
   memset(&ds, 0, sizeof(ds));
   strcpy(ds.custom_timing, "auto");
   if (!win32_modeline_pstrip_create(&b, "\\\\.\\DISPLAY1", &ds))
   {
      fprintf(stderr, "FAIL: PowerStrip create did not find TPShidden\n");
      return 1;
   }
   printf("[pass] PowerStrip: running instance found through its window\n");

   /* get_timing reports the current desktop timing for its own mode */
   memset(&listed, 0, sizeof(listed));
   listed.width = 1280; listed.height = 720; listed.refresh = 60;
   if (!b.get_timing(b.ctx, &listed) || listed.htotal != 1280 + 110 + 40 + 220
         || listed.pclock != 74250000ull)
   {
      fprintf(stderr, "FAIL: PowerStrip get_timing: htotal %d pclock %llu\n",
            listed.htotal, (unsigned long long)listed.pclock);
      return 1;
   }

   make_timing(&m);
   s_ps_sets = 0;
   if (!b.update_mode(b.ctx, &m))
   {
      fprintf(stderr, "FAIL: PowerStrip update_mode\n");
      return 1;
   }
   /* The dotclock probe sets five candidates around the wanted clock,
    * then the winner: the last string sent is the timing on the wire */
   if (s_ps_sets < 2 || sscanf(s_ps_timing, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &a, &hfp, &hsw, &hbp, &e, &vfp, &vsw, &vbp, &pclk, &fl) != 10)
   {
      fprintf(stderr, "FAIL: PowerStrip: %d set(s), last '%s'\n", s_ps_sets, s_ps_timing);
      return 1;
   }
   if (a != m.hactive || hfp != m.hbegin - m.hactive || hsw != m.hend - m.hbegin
         || hbp != m.htotal - m.hend || e != m.vactive || vfp != m.vbegin - m.vactive
         || vsw != m.vend - m.vbegin || vbp != m.vtotal - m.vend
         || pclk != (int)(m.pclock / 1000)
         || !!(fl & 0x02) != !m.hsync || !!(fl & 0x04) != !m.vsync
         || !!(fl & 0x08) != !!m.interlace)
   {
      fprintf(stderr, "FAIL: PowerStrip string '%s' is not the modeline as porch widths\n",
            s_ps_timing);
      return 1;
   }
   printf("[pass] PowerStrip: generated timing sent as porch widths (%s)\n", s_ps_timing);

   b.close(b.ctx);
   if (strcmp(s_ps_timing, backup))
   {
      fprintf(stderr, "FAIL: PowerStrip close left '%s', backup was '%s'\n",
            s_ps_timing, backup);
      return 1;
   }
   printf("[pass] PowerStrip: close put the backed-up timing back\n");
   DestroyWindow(hwnd);
   return 0;
}

int main(void)
{
   s_verbose = getenv("LIVE_VERBOSE") != NULL;
   setvbuf(stdout, NULL, _IONBF, 0);

   /* Wine creates windows only with a display; the resync helper and
    * the PowerStrip stand-in both need one */
   if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
   {
      puts("[skip] no display for the notification windows");
      return 0;
   }

   if (test_adl())
      return 1;
   if (test_ati())
      return 1;
   if (test_pstrip())
      return 1;
   puts("ALL OK");
   return 0;
}

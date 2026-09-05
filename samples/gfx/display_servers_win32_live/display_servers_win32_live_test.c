/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_win32_live_test.c).
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

/* The Win32 display server's modeline path on a live (Wine-backed)
 * display: open binds the primary display device and parses its PCI
 * id, the vendor paths are asked for and decline where there is no
 * AMD driver or PowerStrip, enum lists the driver's modes with the
 * desktop flagged and every entry a system mode, the engine is asked
 * for a core resolution and can only pick a listed mode (caps 0), set
 * switches through ChangeDisplaySettingsExA and the current settings
 * are read back, and close restores the desktop.
 *
 * Under Wine on an Xorg dummy server the listed modes are the X
 * server's RandR modes and CDS changes them, so the switch is real. On
 * a Windows box with an AMD driver the same binary would take the ADL
 * path; that is reported, not asserted. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/modeline/modeline_list.h"
#include "../../../gfx/common/win32_common.h"

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

/* RetroArch-side symbols the driver refers to outside the modeline
 * path; none of them is reached by this test. */
bool video_display_server_set_resolution(unsigned w, unsigned h,
      int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust)
{
   (void)w; (void)h; (void)int_hz; (void)hz; (void)center; (void)monitor_index;
   (void)xoffset; (void)padjust;
   return false;
}
float video_driver_get_refresh_rate(void) { return 60.0f; }
int win32_change_display_settings(const char *str, void *devmode_data, unsigned flags)
{
   return ChangeDisplaySettingsExA(str, (DEVMODEA*)devmode_data, NULL, flags, NULL);
}
uint8_t win32_get_flags(void) { return 0; }
HWND win32_get_window(void) { return NULL; }
void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id)
{
   (void)data; (void)hm_data;
   if (mon_id)
      *mon_id = 0;
}

static void read_current(int *w, int *h, int *hz)
{
   DEVMODEA dm;
   memset(&dm, 0, sizeof(dm));
   dm.dmSize = sizeof(dm);
   EnumDisplaySettingsExA(NULL, ENUM_CURRENT_SETTINGS, &dm, 0);
   *w  = (int)dm.dmPelsWidth;
   *h  = (int)dm.dmPelsHeight;
   *hz = (int)dm.dmDisplayFrequency;
}

int main(void)
{
   void *data;
   video_modeline_ops_t ops;
   video_modeline_disp_t ds;
   video_modeline_gen_t *gen;
   video_modeline_t *mode;
   video_output_info_t outputs[8];
   int nout, i, pick = -1;
   int dw, dh, dhz, cw, ch, chz;

   s_verbose = getenv("LIVE_VERBOSE") != NULL;
   setvbuf(stdout, NULL, _IONBF, 0);

   read_current(&dw, &dh, &dhz);
   if (dw == 0 || dh == 0)
   {
      puts("[skip] no display device");
      return 0;
   }
   printf("desktop: %dx%d@%d\n", dw, dh, dhz);

   data = dispserv_win32.init();
   if (!data)
   {
      fprintf(stderr, "FAIL: init\n");
      return 1;
   }

   nout = dispserv_win32.modeline_list_outputs(data, outputs, 8);
   if (nout < 1)
   {
      fprintf(stderr, "FAIL: list_outputs returned %d\n", nout);
      return 1;
   }
   printf("[pass] list_outputs: %d, first '%s' %ux%u primary %d\n", nout,
         outputs[0].name, outputs[0].width, outputs[0].height, outputs[0].primary);

   memset(&ops, 0, sizeof(ops));
   ops.data       = data;
   ops.open       = dispserv_win32.modeline_open;
   ops.close      = dispserv_win32.modeline_close;
   ops.caps       = dispserv_win32.modeline_caps;
   ops.enum_modes = dispserv_win32.modeline_enum;
   ops.add        = dispserv_win32.modeline_add;
   ops.update     = dispserv_win32.modeline_update;
   ops.del        = dispserv_win32.modeline_delete;
   ops.set        = dispserv_win32.modeline_set;
   ops.flush      = dispserv_win32.modeline_flush;
   ops.name       = "win32";

   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "auto");
   if (!ops.open(data, &ds))
   {
      fprintf(stderr, "FAIL: modeline_open on the primary display\n");
      return 1;
   }
   printf("[pass] open: primary device bound, caps %u (%s)\n",
         ops.caps(data), ops.caps(data) ? "vendor timing path" : "listed modes only");

   gen = modeline_gen_new();
   if (!gen)
      return 1;
   /* With caps 0 every listed mode is a system mode with no timing
    * behind it, and the engine locks those by default (the desktop
    * stays as the fallback). A switchres.ini with lock_system_modes 0
    * and a range wide enough to accept any listed 60 Hz mode is what
    * a user of the CDS-only path sets; the same two options here. */
   modeline_set_option(gen, "monitor", "custom");
   modeline_set_option(gen, "crt_range0",
         "15000-150000, 50-61, 0.5, 0.5, 0.5, 0.05, 0.05, 0.1, 0, 0, 200, 2000, 0, 0");
   modeline_set_option(gen, "lock_system_modes", "0");
   modeline_parse_options(gen);
   if (!modeline_list_init(gen, &ops))
   {
      fprintf(stderr, "FAIL: modeline_list_init\n");
      return 1;
   }
   if (gen->num_modes < 2 || !(gen->desktop_mode.type & MODELINE_DESKTOP))
   {
      fprintf(stderr, "FAIL: enum listed %d modes, desktop flagged %d\n",
            gen->num_modes, !!(gen->desktop_mode.type & MODELINE_DESKTOP));
      return 1;
   }
   if (gen->desktop_mode.width != dw || gen->desktop_mode.height != dh)
   {
      fprintf(stderr, "FAIL: desktop entry is %dx%d, current settings say %dx%d\n",
            gen->desktop_mode.width, gen->desktop_mode.height, dw, dh);
      return 1;
   }
   for (i = 0; i < gen->num_modes; i++)
   {
      if (ops.caps(data) == 0 && !(gen->modes[i].type & MODELINE_TIMING_SYSTEM))
      {
         fprintf(stderr, "FAIL: no vendor path, but mode %d carries a vendor timing\n", i);
         return 1;
      }
      if (pick < 0 && !(gen->modes[i].type & MODELINE_DESKTOP)
            && gen->modes[i].refresh == dhz && gen->modes[i].width < dw)
         pick = i;
   }
   printf("[pass] enum: %d listed modes, desktop %dx%d@%d flagged\n",
         gen->num_modes, gen->desktop_mode.width, gen->desktop_mode.height,
         gen->desktop_mode.refresh);

   if (pick < 0)
   {
      puts("[skip] no smaller listed mode at the desktop rate to switch to");
      ops.close(data);
      dispserv_win32.destroy(data);
      modeline_gen_free(gen);
      return 0;
   }

   /* The consumer's path: get picks the listed mode, flush has
    * nothing to add, set switches through CDS */
   mode = modeline_get(gen, &ops, gen->modes[pick].width, gen->modes[pick].height,
         (double)gen->modes[pick].refresh, 0);
   if (!mode || (mode->type & MODELINE_ADD))
   {
      fprintf(stderr, "FAIL: get did not pick a listed mode for %dx%d@%d\n",
            gen->modes[pick].width, gen->modes[pick].height, gen->modes[pick].refresh);
      return 1;
   }
   if (!modeline_flush(gen, &ops) || !modeline_set(gen, &ops, mode))
   {
      fprintf(stderr, "FAIL: switch to %dx%d@%d\n", mode->width, mode->height, mode->refresh);
      return 1;
   }
   read_current(&cw, &ch, &chz);
   if (cw != mode->width || ch != mode->height)
   {
      fprintf(stderr, "FAIL: current settings read %dx%d@%d after set of %dx%d@%d\n",
            cw, ch, chz, mode->width, mode->height, mode->refresh);
      return 1;
   }
   printf("[pass] set: current settings now %dx%d@%d\n", cw, ch, chz);

   /* Restore and close: the desktop comes back */
   modeline_restore(gen, &ops);
   ops.close(data);
   read_current(&cw, &ch, &chz);
   if (cw != dw || ch != dh)
   {
      fprintf(stderr, "FAIL: after close the settings read %dx%d, desktop was %dx%d\n",
            cw, ch, dw, dh);
      return 1;
   }
   printf("[pass] close: desktop %dx%d@%d back\n", cw, ch, chz);

   dispserv_win32.destroy(data);
   modeline_gen_free(gen);
   puts("ALL OK");
   return 0;
}

/* Video checks, run in Dolphin by os/gekko/tests/run-dolphin.sh:
 * each scan type and standard retraces at the rate its timing says,
 * the retrace callback keeps count, and the console draws. */

#include <stdio.h>
#include <string.h>

#include <gekko/console.h>
#include <gekko/power.h>
#include <gekko/thread.h>
#include <gekko/video.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

static volatile uint32_t cb_count;

static void on_retrace(uint32_t n, void *data)
{
   (void)n;
   (void)data;
   cb_count++;
}

/* Fields per second over 30 retraces, by the time base. */
static double measure(void)
{
   uint32_t start;
   uint64_t t0;
   gk_vi_wait_retrace();
   start = gk_vi_retraces();
   t0    = gk_ticks();
   while (gk_vi_retraces() - start < 30)
      gk_vi_wait_retrace();
   return 30.0 * gk_tb_hz / (double)(gk_ticks() - t0);
}

/* expect: the standard's field rate, 1000 x Hz. */
static void try_mode(const char *name, unsigned std, unsigned scan,
      unsigned w, unsigned lines, unsigned expect)
{
   char what[96];
   double want, got;
   gk_vi_mode_t m;
   memset(&m, 0, sizeof(m));
   m.std      = (uint8_t)std;
   m.scan     = (uint8_t)scan;
   m.fb_width = (uint16_t)w;
   m.width    = 640;
   m.lines    = (uint16_t)lines;
   m.fb_lines = (uint16_t)lines;
   m.x = m.y  = -1;
   want = gk_vi_refresh_hz(&m);
   if (gk_vi_configure(&m))
   {
      snprintf(what, sizeof(what), "%s: configure", name);
      CHECK(0, what);
      return;
   }
   got = measure();
   snprintf(what, sizeof(what), "%s: %.3f Hz, timing says %.3f", name,
         got, want);
   CHECK(got > expect * 0.000999 && got < expect * 0.001001
         && want > expect * 0.000999 && want < expect * 0.001001, what);
}

int main(int argc, char **argv)
{
   gk_vi_mode_t pref;
   void *xfb;
   uint32_t before;
   (void)argc;
   (void)argv;

   gk_vi_preferred(&pref);
   gk_vi_set_retrace_cb(on_retrace, NULL);
   CHECK(gk_vi_configure(&pref) == 0, "configure the preferred mode");
   xfb = gk_vi_alloc_fb(&pref);
   gk_console_attach(xfb, &pref);
   gk_vi_set_fb(xfb);
   gk_vi_set_black(0);
   gk_debug_printf("vi test: std %u scan %u, %ux%u, component %d",
         pref.std, pref.scan, pref.width, pref.lines,
         gk_vi_component_cable());

   before = cb_count;
   gk_sleep_us(100000);
   CHECK(cb_count - before >= 4, "retrace callback runs every field");

   /* 525 lines at 15734.27 Hz, 625 at 15625 Hz; double strike drops
    * the half line: 263 and 312 lines per field. */
   try_mode("NTSC 480i",  GK_VI_NTSC,    GK_VI_INTERLACED,    640, 480, 59940);
   try_mode("NTSC 240p",  GK_VI_NTSC,    GK_VI_DOUBLE_STRIKE, 640, 240, 59826);
   try_mode("NTSC 480p",  GK_VI_NTSC,    GK_VI_PROGRESSIVE,   640, 480, 59940);
   try_mode("PAL 576i",   GK_VI_PAL,     GK_VI_INTERLACED,    640, 574, 50000);
   try_mode("PAL 288p",   GK_VI_PAL,     GK_VI_DOUBLE_STRIKE, 640, 286, 50080);
   try_mode("EURGB60",    GK_VI_EURGB60, GK_VI_INTERLACED,    640, 480, 59940);
   try_mode("NTSC 512x448 scaled", GK_VI_NTSC, GK_VI_INTERLACED, 512, 448,
         59940);

   /* Back to the console for the verdict. */
   gk_vi_configure(&pref);
   gk_vi_set_fb(xfb);
   gk_vi_set_black(0);
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_sleep_us(500000);
#ifdef HW_RVL
   gk_power_off();
#endif
   return 0;
}

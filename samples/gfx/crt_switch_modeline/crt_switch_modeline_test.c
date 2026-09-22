/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crt_switch_modeline_test.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* What the CRT consumer says when the display server underneath it
 * cannot apply a modeline, and which connector it names when it
 * writes an EDID.
 *
 * Like samples/gfx/display_servers this links the real file rather
 * than mirroring its shape: gfx/video_crt_switch.c and the engine in
 * gfx/modeline/ are compiled in, and the RetroArch-side symbols
 * they reach for are supplied here. The display server is
 * the stub, because a server with no modeline ops is exactly the
 * environment the first case is about and no real one on this host
 * can produce it on demand.
 *
 * WHAT IT PINS
 *
 * 1. A server with no modeline path - Wayland, where every
 *    modeline_* op in dispserv_wl.c is NULL - says so once, at bind,
 *    naming itself. Before that line existed the only output was
 *    "[Modeline] Error switching to 320x240@60" and "[CRT] Engine
 *    failed to switch mode", once per geometry change, for the rest
 *    of the session: the right diagnosis (this display server has no
 *    modeline path) was nowhere, and the user was left reading it as
 *    a broken switchres.ini.
 *
 * 2. The same case does not repeat the failure per switch. The
 *    engine's mode list is still generated - the aspect and timing
 *    the consumer publishes come off it - so only the apply is
 *    skipped.
 *
 * 3. A server that HAS ops and whose set() fails still reports it.
 *    That is the regression the second case can cause: silencing the
 *    no-ops case by skipping the call is only correct if a real
 *    failure still speaks.
 *
 * 4. The lines that describe the context, not the switch, are said
 *    once when the engine starts. crt_engine_init() runs on every
 *    geometry change, so anything logged unguarded there repeats for
 *    as long as a core keeps changing mode.
 *
 * 5. The EDID install hint names the head the monitor index selects,
 *    not the first output in the list.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <boolean.h>

#include "../../../gfx/video_crt_switch.h"
#include "../../../gfx/video_display_server.h"
#include "../../../gfx/modeline/modeline_edid.h"
#include "../../../gfx/video_driver.h"
#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../paths.h"
#include "../../../file_path_special.h"
#include "../../../verbosity.h"

/* ---- the log, captured ---- */

#define LOG_MAX 512
#define LOG_LINE 512

static char log_lines[LOG_MAX][LOG_LINE];
static int  log_count;
static bool log_echo;

static void log_put(const char *prefix, const char *fmt, va_list ap)
{
   char line[LOG_LINE];
   size_t _len = strlcpy(line, prefix, sizeof(line));
   vsnprintf(line + _len, sizeof(line) - _len, fmt, ap);
   if (log_echo)
      fputs(line, stdout);
   if (log_count < LOG_MAX)
      strlcpy(log_lines[log_count++], line, LOG_LINE);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   log_put("", fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   log_put("[WARN] ", fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   log_put("[ERROR] ", fmt, ap);
   va_end(ap);
}

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   log_put("[DEBUG] ", fmt, ap);
   va_end(ap);
}

static void log_reset(void)
{
   log_count = 0;
}

static int log_hits(const char *needle)
{
   int i;
   int n = 0;
   for (i = 0; i < log_count; i++)
      if (strstr(log_lines[i], needle))
         n++;
   return n;
}

/* ---- the display server, stubbed ---- */

static bool  srv_has_ops;       /* the server offers a modeline table */
static bool  srv_set_ok;        /* ...and its set() succeeds */
static int   srv_nout;
static char  srv_out_name[8][64];
static const char *srv_ident = "wayland";

static bool srv_open(void *data, const video_modeline_disp_t *ds)
{
   (void)data;
   (void)ds;
   return true;
}

static int srv_adds;
static int srv_sets;

static bool srv_add(void *data, video_modeline_t *mode)
{
   (void)data;
   (void)mode;
   srv_adds++;
   return true;
}

static video_modeline_t srv_last_set;

static bool srv_set(void *data, video_modeline_t *mode)
{
   (void)data;
   srv_sets++;
   if (mode)
      srv_last_set = *mode;
   return srv_set_ok;
}

/* One desktop mode, the way a real server enumerates. The lcd preset
 * derives its line count from it, so without one that path cannot be
 * reached at all. */
static bool srv_have_desktop;

static int srv_enum(void *data, video_modeline_t *modes, int max)
{
   (void)data;
   if (!srv_have_desktop || max < 1)
      return 0;
   memset(&modes[0], 0, sizeof(modes[0]));
   modes[0].pclock  = 148500000;
   modes[0].vfreq   = 60.0;
   modes[0].hfreq   = 67500.0;
   modes[0].width   = 1920;
   modes[0].height  = 1080;
   modes[0].refresh = 60;
   modes[0].hactive = 1920;
   modes[0].hbegin  = 2008;
   modes[0].hend    = 2052;
   modes[0].htotal  = 2200;
   modes[0].vactive = 1080;
   modes[0].vbegin  = 1084;
   modes[0].vend    = 1089;
   modes[0].vtotal  = 1125;
   modes[0].hsync   = 1;
   modes[0].vsync   = 1;
   modes[0].type    = MODELINE_DESKTOP;
   return 1;
}

bool video_display_server_get_modeline_ops(struct video_modeline_ops *ops)
{
   if (!srv_has_ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   ops->data = (void*)&srv_has_ops;
   ops->open = srv_open;
   ops->add  = srv_add;
   ops->enum_modes = srv_enum;
   ops->set  = srv_set;
   ops->name = srv_ident;
   return true;
}

const char *video_display_server_get_ident(void)
{
   return srv_ident;
}

int video_display_server_list_outputs(video_output_info_t *out, int max)
{
   int i;
   if (srv_nout <= 0)
      return -1;
   for (i = 0; i < srv_nout && i < max; i++)
   {
      memset(&out[i], 0, sizeof(out[i]));
      strlcpy(out[i].name, srv_out_name[i], sizeof(out[i].name));
      out[i].dims = VIDEO_SCALE_PACK(640, 480);
      out[i].primary = (i == 0);
   }
   return i;
}

/* A panel reporting 50-75 Hz over 30-85 kHz in its range
 * descriptor, which is what any modern display carries. Built with
 * the tree's own generator so the block the parser sees is the one
 * the writer emits. */
static bool srv_have_edid;

int video_display_server_get_edid(uint8_t *out, size_t max)
{
   video_modeline_t mode;
   video_modeline_range_t range;

   if (!srv_have_edid || max < MODELINE_EDID_SIZE)
      return -1;

   memset(&mode, 0, sizeof(mode));
   mode.pclock  = 148500000;
   mode.vfreq   = 60.0;
   mode.hfreq   = 67500.0;
   mode.width   = 1920;
   mode.height  = 1080;
   mode.refresh = 60;
   mode.hactive = 1920;
   mode.hbegin  = 2008;
   mode.hend    = 2052;
   mode.htotal  = 2200;
   mode.vactive = 1080;
   mode.vbegin  = 1084;
   mode.vend    = 1089;
   mode.vtotal  = 1125;
   mode.hsync   = 1;
   mode.vsync   = 1;

   memset(&range, 0, sizeof(range));
   range.hfreq_min = 30000.0;
   range.hfreq_max = 85000.0;
   range.vfreq_min = 50.0;
   range.vfreq_max = 75.0;
   range.progressive_lines_min = 200;
   range.progressive_lines_max = 1200;

   if (!modeline_edid_build(&mode, &range, "panel", out))
      return -1;
   return MODELINE_EDID_SIZE;
}

/* ---- the rest of the RetroArch side ---- */

static settings_t           stub_settings;
static video_driver_state_t stub_video_st;
static runloop_state_t      stub_runloop_st;

settings_t *config_get_ptr(void) { return &stub_settings; }
video_driver_state_t *video_state_get_ptr(void) { return &stub_video_st; }
runloop_state_t *runloop_state_get_ptr(void) { return &stub_runloop_st; }

bool command_event(enum event_command action, void *data)
{
   (void)action;
   (void)data;
   return true;
}

unsigned int retroarch_get_rotation(void) { return 0; }
float video_driver_get_aspect_ratio(void) { return 4.0f / 3.0f; }
void video_monitor_set_refresh_rate(float hz) { (void)hz; }
void video_driver_set_output_dims(unsigned dims)
{
   (void)dims;
}

unsigned video_driver_get_output_dims(void)
{
   return VIDEO_SCALE_PACK(640, 480);
}

static const char *ctx_ident = "wl";

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   if (ident)
      ident->ident = ctx_ident;
   return true;
}

const char *path_get(enum rarch_path_type type)
{
   (void)type;
   return "";
}

bool fill_pathname_application_data(char *s, size_t len)
{
   const char *cfg = getenv("XDG_CONFIG_HOME");
   if (!cfg)
      return false;
   strlcpy(s, cfg, len);
   return true;
}

size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{
   (void)type;
   if (len)
      s[0] = '\0';
   return 0;
}

/* ---- checks ---- */

static int fails;

static void check(const char *what, bool ok)
{
   if (!ok)
      fails++;
   printf("%s %s\n", ok ? "[pass]" : "[FAIL]", what);
}

static void check_eq(const char *what, int got, int want)
{
   bool ok = (got == want);
   if (!ok)
      fails++;
   printf("%s %-58s got %d want %d\n", ok ? "[pass]" : "[FAIL]", what,
         got, want);
}

/* 15 kHz, three geometry changes, as a core that changes mode does */
static void run_switches(videocrt_switch_t *p_switch, int monitor_index)
{
   crt_switch_res_core(p_switch, 320, 320, 240, 59.94f, false,
         CRT_SWITCH_15KHZ, 0, 0, monitor_index, false, 0, false,
         ASPECT_RATIO_CORE, 0);
   crt_switch_res_core(p_switch, 256, 256, 224, 60.10f, false,
         CRT_SWITCH_15KHZ, 0, 0, monitor_index, false, 0, false,
         ASPECT_RATIO_CORE, 0);
   crt_switch_res_core(p_switch, 384, 384, 224, 59.64f, false,
         CRT_SWITCH_15KHZ, 0, 0, monitor_index, false, 0, false,
         ASPECT_RATIO_CORE, 0);
}

static void test_no_modeline_path(void)
{
   videocrt_switch_t sw;

   printf("\n-- a display server with no modeline path --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops = false;
   srv_ident   = "wayland";
   log_reset();

   run_switches(&sw, 0);

   check_eq("says so once, at bind",
         log_hits("has no modeline path"), 1);
   check("names the server",
         log_hits("\"wayland\"") >= 1);
   check_eq("no per-switch apply failure",
         log_hits("Engine failed to switch mode"), 0);
   check_eq("no per-switch engine error",
         log_hits("[Modeline] Error switching"), 0);
   check("the mode list is still generated",
         log_hits("[CRT] Setting aspect ratio") >= 1);

   crt_destroy_modes(&sw);
}

static void test_khr_display(void)
{
   videocrt_switch_t sw;

   printf("\n-- the Vulkan direct-to-display context --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops = true;   /* a server is up; khr still cannot use it */
   srv_set_ok  = true;
   srv_ident   = "x11";
   ctx_ident   = "khr_display";
   log_reset();

   run_switches(&sw, 0);

   check_eq("says so once, at bind",
         log_hits("Vulkan direct-to-display cannot modeswitch"), 1);
   check_eq("the context line is not repeated either",
         log_hits("Vulkan context detected"), 1);
   check_eq("and does not claim an apply failure",
         log_hits("Engine failed to switch mode"), 0);

   crt_destroy_modes(&sw);
   ctx_ident = "wl";
}

static void test_context_lines_are_said_once(void)
{
   videocrt_switch_t sw;

   printf("\n-- the KMS context, three geometry changes --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops = true;
   srv_set_ok  = true;
   srv_ident   = "kms";
   ctx_ident   = "kms";
   log_reset();

   run_switches(&sw, 0);

   check_eq("the context is named once, not per switch",
         log_hits("[CRT] Video context is:"), 1);
   check_eq("so is the engine-alive line",
         log_hits("KMS context detected"), 1);
   check("and each switch still reports its own resolution",
         log_hits("[CRT] Requested resolution:") == 3);

   crt_destroy_modes(&sw);
   ctx_ident = "wl";
}

/* Loading content tears the display server down and builds it again
 * (content_load is MAIN_DEINIT plus retroarch_main_init), and the
 * engine is kept alive across it. The rebind does not re-enumerate,
 * and a mode already flushed is not flushed again, so the new
 * instance is asked to switch to a mode it was never handed.
 *
 * That holds today because no backend needs the handover: X11 looks
 * the mode up in the X server, which still has the RRMode; KMS reads
 * the timing straight out of the struct; the Win32 backends registered
 * it with the driver. What every one of them relies on is the mode
 * carrying its whole timing at set() time, so that is what is pinned
 * here - a backend that started needing its own add() first, or a
 * mode that arrived at the new instance hollow, would both show up.
 */
static void test_rebind_after_display_server_rebuild(void)
{
   videocrt_switch_t sw;

   printf("\n-- the display server is rebuilt underneath the engine --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops = true;
   srv_set_ok  = true;
   srv_ident   = "x11";
   ctx_ident   = "x11";
   log_reset();

   srv_adds = srv_sets = 0;
   run_switches(&sw, 0);
   check("the first instance was given a mode", srv_adds > 0);
   check("...and asked to switch to it", srv_sets > 0);

   /* The instance goes away and a new one comes up */
   crt_switch_display_server_lost(&sw, (void*)&srv_has_ops);

   srv_adds = srv_sets = 0;
   memset(&srv_last_set, 0, sizeof(srv_last_set));
   log_reset();
   run_switches(&sw, 0);

   check("the engine rebound", log_hits("Rebound to display server") >= 1);
   check("the new instance was asked to switch", srv_sets > 0);
   check("the timing it got is whole, not a handle into the old one",
         srv_last_set.pclock > 0 && srv_last_set.htotal > 0
         && srv_last_set.vtotal > 0 && srv_last_set.hactive > 0
         && srv_last_set.vactive > 0);
   check_eq("and the switch went through",
         log_hits("Engine failed to switch mode"), 0);

   crt_destroy_modes(&sw);
   ctx_ident = "wl";
}

/* The engine is display-agnostic, and the EDID preset takes its
 * limits from whatever the display reports - so a display that says
 * it syncs to 75 Hz gets the content's rate, not the desktop's. That
 * is the whole of "70 Hz DOS content shown at 70 Hz" on a modern
 * panel, and it needs no CRT and no switchres.ini. Nobody would
 * notice it breaking, because nothing in the menu says it is there.
 */
static void test_edid_preset_matches_content_refresh(void)
{
   videocrt_switch_t sw;

   printf("\n-- 70 Hz content on a panel that reports 50-75 Hz --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops   = true;
   srv_set_ok    = true;
   srv_have_edid = true;
   srv_ident     = "x11";
   ctx_ident     = "x11";
   log_reset();
   srv_sets = 0;
   memset(&srv_last_set, 0, sizeof(srv_last_set));

   crt_switch_res_core(&sw, 640, 640, 400, 70.086f, false,
         CRT_SWITCH_EDID, 0, 0, 0, false, 0, false, ASPECT_RATIO_CORE, 0);

   check("the display's own ranges were used",
         log_hits("range(s) from the display's EDID") >= 1);
   check("a mode was applied", srv_sets > 0);
   check("at the content's rate, not the desktop's",
         srv_last_set.vfreq > 69.5 && srv_last_set.vfreq < 70.5);

   crt_destroy_modes(&sw);
   srv_have_edid = false;
   ctx_ident     = "wl";
}

/* The other half of the same idea: keep the panel's own line count
 * and move only the rate. The band it may move within is seeded from
 * the display's EDID, because the preset's own default is the desktop
 * rate plus or minus one, which switches nothing. */
static void test_lcd_preset_keeps_native_lines(void)
{
   videocrt_switch_t sw;

   printf("\n-- 70 Hz content, refresh only, on a 1080p panel --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops      = true;
   srv_set_ok       = true;
   srv_have_edid    = true;
   srv_have_desktop = true;
   srv_ident        = "x11";
   ctx_ident        = "x11";
   log_reset();
   srv_sets = 0;
   memset(&srv_last_set, 0, sizeof(srv_last_set));

   crt_switch_res_core(&sw, 640, 640, 400, 70.086f, false,
         CRT_SWITCH_LCD, 0, 0, 0, false, 0, false, ASPECT_RATIO_CORE, 0);

   check("the refresh band came off the EDID",
         log_hits("Refresh band 50-75 Hz") >= 1);
   check("a mode was applied", srv_sets > 0);
   check("at the content's rate",
         srv_last_set.vfreq > 69.5 && srv_last_set.vfreq < 70.5);
   check("keeping the panel's line count",
         srv_last_set.vactive == 1080);

   crt_destroy_modes(&sw);
   srv_have_edid    = false;
   srv_have_desktop = false;
   ctx_ident        = "wl";
}

static void test_failing_set_still_reports(void)
{
   videocrt_switch_t sw;

   printf("\n-- a server whose set() fails --\n");
   memset(&sw, 0, sizeof(sw));
   srv_has_ops = true;
   srv_set_ok  = false;
   srv_ident   = "x11";
   log_reset();

   run_switches(&sw, 0);

   check("a real apply failure still speaks",
         log_hits("Engine failed to switch mode") >= 1);
   check_eq("and the no-ops line is not used for it",
         log_hits("has no modeline path"), 0);

   crt_destroy_modes(&sw);
}

static void test_edid_hint_names_the_selected_head(void)
{
   char path[4096];

   printf("\n-- the EDID install hint --\n");
   srv_has_ops = true;
   srv_set_ok  = true;
   srv_ident   = "x11";
   srv_nout    = 3;
   strlcpy(srv_out_name[0], "DP-1", sizeof(srv_out_name[0]));
   strlcpy(srv_out_name[1], "HDMI-A-1", sizeof(srv_out_name[1]));
   strlcpy(srv_out_name[2], "VGA-1", sizeof(srv_out_name[2]));

   stub_settings.uints.crt_switch_resolution = CRT_SWITCH_15KHZ;

   /* Monitor Index 3 of three heads */
   stub_settings.uints.video_monitor_index = 3;
   log_reset();
   check("writes a block for the preset",
         crt_switch_write_edid(path, sizeof(path)));
   check_eq("names the third head, not the first",
         log_hits("drm.edid_firmware=VGA-1:"), 1);
   check_eq("and not the first head",
         log_hits("drm.edid_firmware=DP-1:"), 0);
   check("the debugfs route names it too",
         log_hits("/VGA-1/edid_override") >= 1);

   /* "auto" cannot pick a head among three */
   stub_settings.uints.video_monitor_index = 0;
   log_reset();
   crt_switch_write_edid(path, sizeof(path));
   check_eq("auto among three heads stays unnamed",
         log_hits("drm.edid_firmware=<connector>:"), 1);

   /* "auto" on a single head is unambiguous */
   srv_nout = 1;
   strlcpy(srv_out_name[0], "VGA-1", sizeof(srv_out_name[0]));
   log_reset();
   crt_switch_write_edid(path, sizeof(path));
   check_eq("auto on one head names it",
         log_hits("drm.edid_firmware=VGA-1:"), 1);
}

int main(int argc, char **argv)
{
   int i;
   char cfg[1024];
   const char *tmp = getenv("TMPDIR");

   for (i = 1; i < argc; i++)
      if (!strcmp(argv[i], "--verbose"))
         log_echo = true;

   /* The EDID case writes a block; keep it out of the real config */
   snprintf(cfg, sizeof(cfg), "%s/crt_switch_modeline_test",
         tmp ? tmp : "/tmp");
   setenv("XDG_CONFIG_HOME", cfg, 1);

   /* The aspect the consumer publishes is only the engine's when the
    * core provides it, which is how CRT switching is used */
   stub_settings.uints.video_aspect_ratio_idx = ASPECT_RATIO_CORE;

   test_no_modeline_path();
   test_khr_display();
   test_context_lines_are_said_once();
   test_rebind_after_display_server_rebuild();
   test_edid_preset_matches_content_refresh();
   test_lcd_preset_keeps_native_lines();
   test_failing_set_still_reports();
   test_edid_hint_names_the_selected_head();

   printf("\n%s\n", fails ? "FAILED" : "all checks passed");
   return fails ? 1 : 0;
}

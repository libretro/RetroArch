/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dispserv_gx_test.c).
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

/* Contract test for the GameCube/Wii display server
 * (gfx/display_servers/dispserv_gx.c + dispserv_gx_modes.c), linked
 * as they ship against stubbed libogc queries.
 *
 * On every TV standard the console can be set to (NTSC, NTSC with
 * progressive scan, PAL 50, EuRGB60, MPAL) it checks that:
 *  - the resolution list offers only modes the standard shows as they
 *    are, with exactly one marked current, and the console's own
 *    preferred mode is always in it (PAL's 640x574 is in no table
 *    entry, so it comes in as the default);
 *  - each entry, taken through the menu's own round trip - the label
 *    menu_displaylist.c prints for the dropdown, parsed back the way
 *    action_cb_push_dropdown_item_resolution does - selects exactly
 *    that entry: current_resolution_id becomes its id, the driver is
 *    handed the table mode for it, and the refresh rate reported is
 *    the one listed. 240/288-line modes are double strike at the
 *    non-interlaced field rate (59.8261/50.0801 Hz), taller ones
 *    interlaced unless progressive;
 *  - a set_resolution with no dims (the refresh rate autoswitch) and
 *    one naming a mode the standard lacks both fail without touching
 *    the mode or the stored id;
 *  - a stored id past the table's end reads back as the default;
 *  - the ids the config stores (current_resolution_id) keep meaning
 *    the modes they meant before the table moved here. */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gccore.h>
#include <ogc/conf.h>

#include "gfx/display_servers/dispserv_gx.h"
#include "gfx/video_driver.h"
#include "retroarch.h"

static int failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { \
      failures++; \
      printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      printf(__VA_ARGS__); \
      printf("\n"); \
   } \
} while (0)

/* ---- stubbed console ---- */

static int      conf_video;
static int      conf_eurgb60;
static int      conf_progressive;
static u32      component_cable;
static unsigned pref_w, pref_h;

int CONF_GetVideo(void)            { return conf_video; }
int CONF_GetEuRGB60(void)          { return conf_eurgb60; }
int CONF_GetProgressiveScan(void)  { return conf_progressive; }
u32 VIDEO_HaveComponentCable(void) { return component_cable; }
u32 VIDEO_GetCurrentTvMode(void)   { return VI_NTSC; }

GXRModeObj *VIDEO_GetPreferredMode(GXRModeObj *mode)
{
   memset(mode, 0, sizeof(*mode));
   mode->fbWidth   = (u16)pref_w;
   mode->efbHeight = (u16)(pref_h > 480 ? 480 : pref_h);
   mode->xfbHeight = (u16)pref_h;
   return mode;
}

/* ---- stubbed RetroArch ---- */

static global_t g_global;
static unsigned set_mode_calls;
static unsigned set_mode_dims;

global_t *global_get_ptr(void) { return &g_global; }

bool video_driver_set_video_mode(unsigned dims, bool fullscreen)
{
   set_mode_calls++;
   set_mode_dims = dims;
   return true;
}

void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

/* ---- the menu's round trip ---- */

/* menu_displaylist.c, DISPLAYLIST_DROPDOWN_LIST_RESOLUTION */
static void menu_label(char *s, size_t len, const video_display_config_t *c)
{
   snprintf(s, len, "%dx%d (%.3f Hz)%s%s",
         VIDEO_SCALE_W(c->dims), VIDEO_SCALE_H(c->dims),
         c->refreshrate_float,
         c->interlaced ? "[i]" : "",
         c->dblscan    ? "[d]" : "");
}

/* menu_cbs_ok.c, action_cb_push_dropdown_item_resolution. It used to
 * parse inside VIDEO_SCALE_PUT_W/H, whose PACK evaluates each argument
 * twice: the height's strtoul() ran again from where the first call
 * had left 'end' and came back 0, so every pick went out as Wx0 and
 * every server refused it. This test caught that; the CI step also
 * greps menu_cbs_ok.c so the shape cannot come back. */
static bool menu_parse(const char *path, unsigned *dims, float *hz)
{
   char *end = NULL;
   unsigned width, height;
   width     = (unsigned)strtoul(path, &end, 0);
   if (end == path || *end != 'x')
      return false;
   ++end;
   height    = (unsigned)strtoul(end, &end, 0);
   *dims     = VIDEO_SCALE_PACK(width, height);
   while (*end == ' ' || *end == '(')
      ++end;
   *hz = (float)strtod(end, NULL);
   return true;
}

/* ---- tests ---- */

typedef struct
{
   const char *name;
   int         video;
   int         eurgb60;
   int         progressive;
   unsigned    pref_w, pref_h;
   unsigned    max_h;
   bool        fifty;
   unsigned    list_count;
   unsigned    default_idx_dims; /* what id 0 lists as */
} standard_t;

static const standard_t standards[] = {
   { "NTSC",        CONF_VIDEO_NTSC, 0, 0, 640, 480, 480, false, 39, 0 },
   { "NTSC 480p",   CONF_VIDEO_NTSC, 0, 1, 640, 480, 480, false, 39, 0 },
   { "PAL 50",      CONF_VIDEO_PAL,  0, 0, 640, 574, 576, true,  40, 0 },
   { "PAL EuRGB60", CONF_VIDEO_PAL,  1, 0, 640, 480, 480, false, 39, 0 },
   { "MPAL",        CONF_VIDEO_MPAL, 0, 0, 640, 480, 480, false, 39, 0 },
};

static void set_standard(const standard_t *st)
{
   conf_video       = st->video;
   conf_eurgb60     = st->eurgb60;
   conf_progressive = st->progressive;
   component_cable  = st->progressive ? 1 : 0;
   pref_w           = st->pref_w;
   pref_h           = st->pref_h;
}

static video_display_config_t *get_list(void *data, unsigned *n)
{
   *n = 0;
   return (video_display_config_t*)dispserv_gx.get_resolution_list(data, n);
}

static unsigned count_current(const video_display_config_t *l, unsigned n,
      unsigned *which)
{
   unsigned i, c = 0;
   for (i = 0; i < n; i++)
      if (l[i].current)
      {
         c++;
         *which = i;
      }
   return c;
}

static void test_table_ids(void)
{
   /* The config's current_resolution_id values, as gx_gfx.c's
    * GX_RESOLUTIONS_* enum numbered them */
   CHECK(gx_modes_count() == 40, "table has %u ids", gx_modes_count());
   CHECK(gx_modes_dims(0)  == 0, "id 0 is not the default");
   CHECK(gx_modes_dims(1)  == VIDEO_SCALE_PACK(512, 192), "id 1");
   CHECK(gx_modes_dims(20) == VIDEO_SCALE_PACK(640, 240), "id 20");
   CHECK(gx_modes_dims(23) == VIDEO_SCALE_PACK(640, 400), "id 23");
   CHECK(gx_modes_dims(30) == VIDEO_SCALE_PACK(640, 448), "id 30");
   CHECK(gx_modes_dims(39) == VIDEO_SCALE_PACK(640, 480), "id 39");
   CHECK(gx_modes_dims(40) == 0, "id past the end is not 0");
   CHECK(gx_modes_clamp_id(40) == 0 && gx_modes_clamp_id(39) == 39,
         "clamp");
}

static void test_standard(void *data, const standard_t *st)
{
   unsigned i, n, which = 0;
   video_display_config_t *list;

   set_standard(st);
   g_global.console.screen.resolutions.current.id = 0;

   list = get_list(data, &n);
   CHECK(list != NULL, "%s: no list", st->name);
   if (!list)
      return;

   CHECK(n == st->list_count, "%s: %u entries, want %u",
         st->name, n, st->list_count);
   CHECK(count_current(list, n, &which) == 1,
         "%s: default is not exactly one current entry", st->name);
   CHECK(list[which].dims == VIDEO_SCALE_PACK(st->pref_w, st->pref_h),
         "%s: default lists as %ux%u", st->name,
         VIDEO_SCALE_W(list[which].dims), VIDEO_SCALE_H(list[which].dims));

   for (i = 0; i < n; i++)
   {
      char label[64];
      unsigned dims, m, cur = 0;
      float hz;
      unsigned h = VIDEO_SCALE_H(list[i].dims);
      bool ds    = h <= st->max_h / 2;
      float want = st->fifty ? (ds ? 50.0801f : 50.0f)
                             : (ds ? 59.8261f : 59.94f);
      video_display_config_t *after;

      CHECK(h <= st->max_h && VIDEO_SCALE_W(list[i].dims) <= 720,
            "%s: %ux%u does not fit", st->name,
            VIDEO_SCALE_W(list[i].dims), h);
      CHECK(fabsf(list[i].refreshrate_float - want) < 0.0001f,
            "%s: %ux%u lists %.4f Hz, want %.4f", st->name,
            VIDEO_SCALE_W(list[i].dims), h, list[i].refreshrate_float, want);
      CHECK(list[i].interlaced == (!ds && !st->progressive),
            "%s: %ux%u interlaced=%d", st->name,
            VIDEO_SCALE_W(list[i].dims), h, list[i].interlaced);

      menu_label(label, sizeof(label), &list[i]);
      CHECK(menu_parse(label, &dims, &hz), "%s: unparsable '%s'",
            st->name, label);

      set_mode_calls = 0;
      CHECK(dispserv_gx.set_resolution(data, dims, (int)floor(hz), hz,
               0, 0, 0, 0),
            "%s: '%s' refused", st->name, label);
      CHECK(set_mode_calls == 1, "%s: '%s' set the mode %u times",
            st->name, label, set_mode_calls);
      CHECK(g_global.console.screen.resolutions.current.id == list[i].idx,
            "%s: '%s' stored id %u, want %u", st->name, label,
            g_global.console.screen.resolutions.current.id, list[i].idx);
      CHECK(set_mode_dims == gx_modes_dims(list[i].idx),
            "%s: '%s' handed the driver %ux%u", st->name, label,
            VIDEO_SCALE_W(set_mode_dims), VIDEO_SCALE_H(set_mode_dims));
      CHECK(fabsf(dispserv_gx.get_refresh_rate(data)
               - list[i].refreshrate_float) < 0.0001f,
            "%s: '%s' reports another refresh rate", st->name, label);

      after = get_list(data, &m);
      CHECK(after && m == n, "%s: list changed size", st->name);
      if (after)
      {
         CHECK(count_current(after, m, &cur) == 1 && cur == i,
               "%s: after '%s' entry %u is current, want %u",
               st->name, label, cur, i);
         free(after);
      }
   }

   free(list);
}

static void test_refusals(void *data)
{
   unsigned dims = 0;
   set_standard(&standards[0]);
   g_global.console.screen.resolutions.current.id = 20;

   set_mode_calls = 0;
   /* video_display_server_set_refresh_rate() passes no dims */
   CHECK(!dispserv_gx.set_resolution(data, 0, 50, 50.0f, 0, 0, 0, 0),
         "refresh-rate-only switch accepted");
   CHECK(!dispserv_gx.set_resolution(data, VIDEO_SCALE_PACK(800, 600),
            60, 60.0f, 0, 0, 0, 0), "800x600 accepted");
   /* PAL's preferred mode, on NTSC */
   CHECK(!dispserv_gx.set_resolution(data, VIDEO_SCALE_PACK(640, 574),
            50, 50.0f, 0, 0, 0, 0), "640x574 accepted on NTSC");
   CHECK(set_mode_calls == 0, "a refused switch touched the mode");
   CHECK(g_global.console.screen.resolutions.current.id == 20,
         "a refused switch changed the stored id");

   /* The mode from issue 19380 */
   CHECK(dispserv_gx.set_resolution(data, VIDEO_SCALE_PACK(640, 240),
            59, 59.826f, 0, 0, 0, 0)
         && g_global.console.screen.resolutions.current.id == 20
         && set_mode_dims == VIDEO_SCALE_PACK(640, 240),
         "640x240 does not select id 20");

   g_global.console.screen.resolutions.current.id = 250;
   dispserv_gx.get_video_output_size(data, &dims, NULL, 0);
   CHECK(dims == 0 && g_global.console.screen.resolutions.current.id == 0,
         "stored id 250 does not read back as the default");
}

int main(void)
{
   unsigned i;
   void *data = dispserv_gx.init();

   CHECK(data != NULL, "init returned NULL");
   CHECK(dispserv_gx.ident && !strcmp(dispserv_gx.ident, "gx"), "ident");

   test_table_ids();
   for (i = 0; i < sizeof(standards) / sizeof(standards[0]); i++)
      test_standard(data, &standards[i]);
   test_refusals(data);

   dispserv_gx.destroy(data);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] dispserv_gx_test\n");
   return 0;
}

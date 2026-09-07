/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_kms_test.c).
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

/* Regression test for
 * gfx/display_servers/dispserv_kms.c::kms_display_server_get_resolution_list().
 *
 * Two defects, found by auditing the KMS driver for the same
 * shape of bug as the X11 one next door.
 *
 * THE NULL DEREFERENCE
 *
 *   *len = g_drm_connector->count_modes;
 *
 * g_drm_connector is a global owned by gfx/common/drm_common.c
 * and set to NULL by drm_free(), which runs on every context
 * teardown -- a resolution change, a fullscreen toggle, a
 * driver reinit.  The display server object outlives that, so
 * the menu can ask for the resolution list with no connector to
 * describe, and this reads through NULL.
 *
 * What makes it clearly an oversight rather than an assumption:
 * g_drm_mode, the sibling global from the same file, is tested
 * for exactly this a few lines earlier in the same function.
 *
 * THE SORT THAT SORTED NOTHING
 *
 *   unsigned count = 0;
 *   ...
 *   for (i = 0, j = 0; i < g_drm_connector->count_modes; i++)
 *      ... conf[j] ... j++;
 *   ...
 *   qsort(conf, count, sizeof(video_display_config_t), ...);
 *
 * count was declared zero and never assigned; j is the element
 * count.  So the qsort ran over zero elements and the menu's
 * resolution list came back in whatever order the connector
 * reported its modes, silently.  No crash, no diagnostic, just
 * an unsorted list that looks plausible.
 *
 * The test drives the real driver against a fabricated
 * drmModeConnector, which is all the function reads.  The
 * ordering check deliberately hands it modes in an order the
 * comparator will rearrange, so a qsort over the wrong count
 * shows up as output identical to the input.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/video_driver.h"
#include "../../../command.h"

extern const video_display_server_t dispserv_kms;

/* ------------------------------------------------------------------
 * The globals and helpers dispserv_kms.c reaches for.
 * ------------------------------------------------------------------ */

drmModeConnector *g_drm_connector = NULL;
drmModeModeInfo  *g_drm_mode      = NULL;

float drm_calc_refresh_rate(drmModeModeInfo *mode)
{
   if (!mode || mode->htotal == 0 || mode->vtotal == 0)
      return 0.0f;
   return (mode->clock * 1000.0f) / (mode->htotal * (float)mode->vtotal);
}

bool command_event(enum event_command cmd, void *data)
{
   (void)cmd; (void)data;
   return true;
}

void video_monitor_set_refresh_rate(float hz) { (void)hz; }

void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

/* The modeline path: set copies the timing into the CRT consumer's
 * drmModeModeInfo mirror in the video state, then asks the video
 * driver for a mode set of the mode's size. The DRM context reads
 * the mirror on that set. */
static video_driver_state_t s_video_st;
static unsigned s_set_video_mode_calls;
static unsigned s_set_video_mode_w, s_set_video_mode_h;
static bool     s_set_video_mode_fs;

video_driver_state_t *video_state_get_ptr(void)
{
   return &s_video_st;
}

bool video_driver_set_video_mode(unsigned width, unsigned height,
      bool fullscreen)
{
   s_set_video_mode_calls++;
   s_set_video_mode_w  = width;
   s_set_video_mode_h  = height;
   s_set_video_mode_fs = fullscreen;
   return true;
}

/* ------------------------------------------------------------------
 * A connector with the given modes, in the given order.
 * ------------------------------------------------------------------ */

typedef struct
{
   unsigned w;
   unsigned h;
   unsigned hz;
} mode_spec_t;

static drmModeConnector *make_connector(const mode_spec_t *specs, int n)
{
   drmModeConnector *c = (drmModeConnector*)calloc(1, sizeof(*c));
   int i;

   if (!c)
      return NULL;

   c->count_modes = n;
   if (n > 0)
   {
      if (!(c->modes = (drmModeModeInfo*)calloc(n, sizeof(drmModeModeInfo))))
      {
         free(c);
         return NULL;
      }
      for (i = 0; i < n; i++)
      {
         c->modes[i].hdisplay = (uint16_t)specs[i].w;
         c->modes[i].vdisplay = (uint16_t)specs[i].h;
         c->modes[i].htotal   = (uint16_t)(specs[i].w + 100);
         c->modes[i].vtotal   = (uint16_t)(specs[i].h + 50);
         /* clock is in kHz; pick one that yields the wanted rate. */
         c->modes[i].clock    = (uint32_t)((specs[i].hz
               * (float)c->modes[i].htotal
               * (float)c->modes[i].vtotal) / 1000.0f);
      }
   }
   return c;
}

static void free_connector(drmModeConnector *c)
{
   if (!c)
      return;
   if (c->modes)
      free(c->modes);
   free(c);
}

/* ------------------------------------------------------------------
 * Cases
 * ------------------------------------------------------------------ */

/* The connector can be NULL at any point after a context
 * teardown; asking for the list must not read through it. */
static int test_null_connector(void)
{
   void *data;
   unsigned len = 12345;
   void *list;

   g_drm_connector = NULL;
   g_drm_mode      = NULL;

   if (!(data = dispserv_kms.init()))
   {
      fputs("FAIL: kms display server init returned NULL\n", stderr);
      return 1;
   }

   list = dispserv_kms.get_resolution_list(data, &len);
   dispserv_kms.destroy(data);

   if (list)
   {
      fprintf(stderr,
            "FAIL: a NULL connector produced a resolution list\n");
      free(list);
      return 1;
   }
   if (len != 0)
   {
      fprintf(stderr,
            "FAIL: a NULL connector left len at %u, want 0\n", len);
      return 1;
   }

   printf("[pass] a NULL connector yields an empty list, not a signal\n");
   return 0;
}

/* A connector that reports no modes has nothing to describe;
 * calloc(0) is implementation-defined and the loop bound is
 * signed, so this is worth pinning down separately. */
static int test_connector_without_modes(void)
{
   void *data;
   unsigned len = 12345;
   void *list;

   g_drm_connector = make_connector(NULL, 0);
   g_drm_mode      = NULL;
   if (!g_drm_connector)
      return 1;

   data = dispserv_kms.init();
   list = dispserv_kms.get_resolution_list(data, &len);
   dispserv_kms.destroy(data);

   free_connector(g_drm_connector);
   g_drm_connector = NULL;

   if (list)
   {
      fputs("FAIL: a connector with no modes produced a list\n", stderr);
      free(list);
      return 1;
   }
   if (len != 0)
   {
      fprintf(stderr, "FAIL: no modes left len at %u, want 0\n", len);
      return 1;
   }

   printf("[pass] a connector with no modes yields an empty list\n");
   return 0;
}

/* Every mode the connector reports must come back, once. */
static int test_all_modes_reported(void)
{
   static const mode_spec_t specs[] = {
      { 1920, 1080, 60 },
      { 1280,  720, 60 },
      {  640,  480, 60 },
   };
   const int n = (int)(sizeof(specs) / sizeof(specs[0]));
   video_display_config_t *list;
   void *data;
   unsigned len = 0;
   int i, j;

   g_drm_connector = make_connector(specs, n);
   g_drm_mode      = NULL;
   if (!g_drm_connector)
      return 1;

   data = dispserv_kms.init();
   list = (video_display_config_t*)
      dispserv_kms.get_resolution_list(data, &len);
   dispserv_kms.destroy(data);

   free_connector(g_drm_connector);
   g_drm_connector = NULL;

   if (!list)
   {
      fputs("FAIL: no resolution list for a connector with modes\n", stderr);
      return 1;
   }
   if (len != (unsigned)n)
   {
      fprintf(stderr, "FAIL: reported %u modes, want %d\n", len, n);
      free(list);
      return 1;
   }

   for (i = 0; i < n; i++)
   {
      int found = 0;
      for (j = 0; j < n; j++)
         if (     list[j].width  == specs[i].w
               && list[j].height == specs[i].h)
            found++;
      if (found != 1)
      {
         fprintf(stderr, "FAIL: %ux%u appears %d time(s), want 1\n",
               specs[i].w, specs[i].h, found);
         free(list);
         return 1;
      }
   }

   free(list);
   printf("[pass] every reported mode comes back exactly once\n");
   return 0;
}

/* The list is sorted.  The modes go in descending, and the
 * comparator orders on the "%04dx%04d (%d Hz)" string, so a
 * correct sort has to reverse them; a qsort over the wrong
 * element count leaves them exactly as supplied. */
static int test_list_is_sorted(void)
{
   static const mode_spec_t specs[] = {
      { 1920, 1080, 60 },
      { 1280,  720, 60 },
      {  800,  600, 60 },
      {  640,  480, 60 },
   };
   const int n = (int)(sizeof(specs) / sizeof(specs[0]));
   video_display_config_t *list;
   void *data;
   unsigned len = 0;
   int i;

   g_drm_connector = make_connector(specs, n);
   g_drm_mode      = NULL;
   if (!g_drm_connector)
      return 1;

   data = dispserv_kms.init();
   list = (video_display_config_t*)
      dispserv_kms.get_resolution_list(data, &len);
   dispserv_kms.destroy(data);

   free_connector(g_drm_connector);
   g_drm_connector = NULL;

   if (!list || len != (unsigned)n)
   {
      fputs("FAIL: could not obtain the list to check ordering\n", stderr);
      if (list)
         free(list);
      return 1;
   }

   for (i = 1; i < n; i++)
   {
      if (list[i - 1].width <= list[i].width)
         continue;
      fprintf(stderr,
            "FAIL: entry %d (%ux%u) sorts after entry %d (%ux%u);"
            " the list came back in connector order, so the qsort"
            " ran over the wrong element count\n",
            i - 1, list[i - 1].width, list[i - 1].height,
            i,     list[i].width,     list[i].height);
      free(list);
      return 1;
   }

   free(list);
   printf("[pass] the resolution list is actually sorted\n");
   return 0;
}

/* The mode currently in use is flagged, and only that one. */
static int test_current_mode_flagged(void)
{
   static const mode_spec_t specs[] = {
      { 1920, 1080, 60 },
      { 1280,  720, 60 },
      {  640,  480, 60 },
   };
   const int n = (int)(sizeof(specs) / sizeof(specs[0]));
   video_display_config_t *list;
   void *data;
   unsigned len = 0;
   int i, current = 0;

   g_drm_connector = make_connector(specs, n);
   if (!g_drm_connector)
      return 1;
   /* Pretend the middle mode is live. */
   g_drm_mode = &g_drm_connector->modes[1];

   data = dispserv_kms.init();
   list = (video_display_config_t*)
      dispserv_kms.get_resolution_list(data, &len);
   dispserv_kms.destroy(data);

   if (!list || len != (unsigned)n)
   {
      fputs("FAIL: could not obtain the list to check the current flag\n",
            stderr);
      if (list)
         free(list);
      free_connector(g_drm_connector);
      g_drm_connector = NULL;
      g_drm_mode      = NULL;
      return 1;
   }

   for (i = 0; i < (int)len; i++)
      if (list[i].current)
      {
         current++;
         if (list[i].width != specs[1].w || list[i].height != specs[1].h)
         {
            fprintf(stderr, "FAIL: %ux%u flagged current, want %ux%u\n",
                  list[i].width, list[i].height, specs[1].w, specs[1].h);
            free(list);
            free_connector(g_drm_connector);
            g_drm_connector = NULL;
            g_drm_mode      = NULL;
            return 1;
         }
      }

   free(list);
   free_connector(g_drm_connector);
   g_drm_connector = NULL;
   g_drm_mode      = NULL;

   if (current != 1)
   {
      fprintf(stderr, "FAIL: %d entries flagged current, want 1\n", current);
      return 1;
   }

   printf("[pass] exactly the live mode is flagged current\n");
   return 0;
}

/* ------------------------------------------------------------------
 * The modeline ops: KMS generates freely (caps ADD, nothing listed),
 * add/update/delete succeed without touching anything, set mirrors
 * the timing for the DRM context and requests one mode set.
 * ------------------------------------------------------------------ */

static int test_modeline_ops(void)
{
   void *data;
   video_modeline_disp_t ds;
   video_modeline_t mode;
   video_modeline_t listed[4];
   video_output_info_t outputs[4];
   videocrt_switch_t *mirror = &s_video_st.crt_switch_st;
   const mode_spec_t specs[] = { { 1920, 1080, 60 } };

   g_drm_connector = make_connector(specs, 1);
   if (!g_drm_connector)
      return 1;
   g_drm_connector->connector_id = 77;
   g_drm_mode = &g_drm_connector->modes[0];
   memset(&s_video_st, 0, sizeof(s_video_st));
   s_set_video_mode_calls = 0;

   data = dispserv_kms.init();
   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "dummy");

   if (dispserv_kms.modeline_list_outputs(data, outputs, 4) != 1
         || outputs[0].id != 77 || outputs[0].width != 1920 || !outputs[0].primary)
   {
      fprintf(stderr, "FAIL: list_outputs did not report the live connector\n");
      return 1;
   }
   if (!dispserv_kms.modeline_open(data, &ds))
   {
      fprintf(stderr, "FAIL: modeline_open\n");
      return 1;
   }
   if (dispserv_kms.modeline_caps(data) != MODELINE_CAPS_ADD)
   {
      fprintf(stderr, "FAIL: KMS caps must be ADD only\n");
      return 1;
   }
   if (dispserv_kms.modeline_enum(data, listed, 4) != 0)
   {
      fprintf(stderr, "FAIL: KMS must list nothing: the engine generates\n");
      return 1;
   }

   /* A generated 15 kHz timing */
   memset(&mode, 0, sizeof(mode));
   mode.pclock     = 6514560;
   mode.width      = mode.hactive = 320;
   mode.hbegin     = 333;
   mode.hend       = 364;
   mode.htotal     = 416;
   mode.height     = mode.vactive = 240;
   mode.vbegin     = 242;
   mode.vend       = 245;
   mode.vtotal     = 261;
   mode.vfreq      = 60.0;
   mode.refresh    = 60;
   mode.hsync      = 0;
   mode.vsync      = 1;
   mode.interlace  = 0;
   mode.doublescan = 1;
   mode.type       = MODELINE_ADD;

   if (!dispserv_kms.modeline_add(data, &mode)
         || !dispserv_kms.modeline_update(data, &mode)
         || !dispserv_kms.modeline_delete(data, &mode)
         || !dispserv_kms.modeline_flush(data))
   {
      fprintf(stderr, "FAIL: KMS add/update/delete/flush must be no-op successes\n");
      return 1;
   }
   if (!(mode.type & MODELINE_TIMING_DRMKMS))
   {
      fprintf(stderr, "FAIL: add did not tag the mode as a DRM timing\n");
      return 1;
   }
   if (s_set_video_mode_calls != 0 || mirror->vdisplay != 0)
   {
      fprintf(stderr, "FAIL: staging touched the mirror or the driver\n");
      return 1;
   }

   if (!dispserv_kms.modeline_set(data, &mode))
   {
      fprintf(stderr, "FAIL: modeline_set\n");
      return 1;
   }
   /* The mirror carries the timing the way drmModeModeInfo wants it:
    * clock in kHz, sync counts as given, flags as separate ints */
   if (mirror->clock != 6514 || mirror->hdisplay != 320
         || mirror->hsync_start != 333 || mirror->hsync_end != 364 || mirror->htotal != 416
         || mirror->vdisplay != 240 || mirror->vsync_start != 242
         || mirror->vsync_end != 245 || mirror->vtotal != 261
         || mirror->vrefresh != 60 || mirror->hskew != 0 || mirror->vscan != 0
         || mirror->interlace != 0 || mirror->doublescan != 1
         || mirror->hsync != 0 || mirror->vsync != 1)
   {
      fprintf(stderr, "FAIL: mirror %u %u %u %u %u %u %u %u %u @%u i%d d%d h%d v%d\n",
            mirror->clock, mirror->hdisplay, mirror->hsync_start, mirror->hsync_end,
            mirror->htotal, mirror->vdisplay, mirror->vsync_start, mirror->vsync_end,
            mirror->vtotal, mirror->vrefresh, mirror->interlace, mirror->doublescan,
            mirror->hsync, mirror->vsync);
      return 1;
   }
   if (s_set_video_mode_calls != 1 || s_set_video_mode_w != 320
         || s_set_video_mode_h != 240 || !s_set_video_mode_fs)
   {
      fprintf(stderr, "FAIL: set made %u mode set(s), last %ux%u fs=%d\n",
            s_set_video_mode_calls, s_set_video_mode_w, s_set_video_mode_h,
            s_set_video_mode_fs);
      return 1;
   }

   /* A second set: the mirror is rewritten, never REINIT'd around */
   mode.interlace = 1;
   mode.vactive   = mode.height = 480;
   mode.vbegin    = 483;
   mode.vend      = 489;
   mode.vtotal    = 523;
   mode.pclock    = 13038390;
   if (!dispserv_kms.modeline_set(data, &mode) || mirror->clock != 13038
         || mirror->vdisplay != 480 || mirror->interlace != 1
         || s_set_video_mode_calls != 2)
   {
      fprintf(stderr, "FAIL: second set did not rewrite the mirror\n");
      return 1;
   }

   dispserv_kms.modeline_close(data);
   dispserv_kms.destroy(data);
   free_connector(g_drm_connector);
   g_drm_connector = NULL;
   g_drm_mode      = NULL;

   printf("[pass] modeline set mirrors the timing for the DRM context and requests one mode set\n");
   return 0;
}

int main(void)
{
   if (test_null_connector())
      return 1;
   if (test_modeline_ops())
      return 1;
   if (test_connector_without_modes())
      return 1;
   if (test_all_modes_reported())
      return 1;
   if (test_list_is_sorted())
      return 1;
   if (test_current_mode_flagged())
      return 1;

   puts("ALL OK");
   return 0;
}

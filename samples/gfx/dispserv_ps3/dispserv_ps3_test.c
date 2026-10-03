/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dispserv_ps3_test.c).
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

/* Contract test for the PS3 display server
 * (gfx/display_servers/dispserv_ps3.c + dispserv_ps3_modes.c), linked
 * as they ship against stubbed PSL1GHT video output queries.
 *
 * For displays taking different sets of modes (SD only, a 720p set, a
 * full 1080 set, a PAL SD set), with the system menu in each mode, it
 * checks that:
 *  - the resolution list offers exactly the modes the display takes,
 *    with the one in use marked current: the stored choice when the
 *    display takes it, otherwise the system menu's mode;
 *  - each entry, taken through the menu's own round trip - the label
 *    menu_displaylist.c prints, parsed back the way
 *    action_cb_push_dropdown_item_resolution does - selects exactly
 *    that mode: current_resolution_id becomes its resolution id, the
 *    video driver is reinitialised once (not at all when the mode is
 *    already the one in use), and the refresh rate and output size
 *    reported are the ones listed;
 *  - ps3_display_server_resolution(), which the RSX driver and the
 *    PSGL context configure the output from, gives the choice when
 *    the display takes it and the system menu's mode otherwise -
 *    before this the RSX driver ignored the setting entirely;
 *  - a set_resolution with no dims (the refresh rate autoswitch) and
 *    one naming a mode the display does not take both fail without
 *    touching the stored id or reinitialising anything;
 *  - the ids the config stores keep meaning the modes they did. */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysutil/video.h>

#include "gfx/display_servers/dispserv_ps3.h"
#include "command.h"
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

/* ---- stubbed video output ---- */

static unsigned available_mask; /* bit n: resolution id n */
static unsigned system_id;
static int      state_fails;

int32_t videoGetState(int32_t videoOut, int32_t deviceIndex, videoState *state)
{
   memset(state, 0, sizeof(*state));
   if (state_fails)
      return -1;
   state->displayMode.resolution = (uint8_t)system_id;
   return 0;
}

int32_t videoGetResolutionAvailability(uint32_t videoOut, uint32_t resolutionId,
      uint32_t aspect, uint32_t option)
{
   return (resolutionId < 32 && (available_mask & (1u << resolutionId)))
      ? 1 : 0;
}

/* ---- stubbed RetroArch ---- */

static global_t g_global;
static unsigned reinits;

global_t *global_get_ptr(void) { return &g_global; }

bool command_event(enum event_command action, void *data)
{
   if (action == CMD_EVENT_REINIT)
      reinits++;
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

/* menu_cbs_ok.c, action_cb_push_dropdown_item_resolution */
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

#define BIT(id) (1u << (id))

typedef struct
{
   const char *name;
   unsigned    mask;
   unsigned    count;
} display_t;

static const display_t displays[] = {
   { "SD 480",   BIT(4), 1 },
   { "PAL SD",   BIT(4) | BIT(5), 2 },
   { "HD 720",   BIT(4) | BIT(2), 2 },
   { "Full HD",  BIT(4) | BIT(5) | BIT(2) | BIT(1)
               | BIT(10) | BIT(11) | BIT(12) | BIT(13), 8 },
};

static const unsigned all_ids[] = { 4, 5, 13, 2, 12, 11, 10, 1 };

static video_display_config_t *get_list(void *data, unsigned *n)
{
   *n = 0;
   return (video_display_config_t*)dispserv_ps3.get_resolution_list(data, n);
}

static void test_table_ids(void)
{
   CHECK(ps3_modes_dims(1)  == VIDEO_SCALE_PACK(1920, 1080), "id 1");
   CHECK(ps3_modes_dims(2)  == VIDEO_SCALE_PACK(1280,  720), "id 2");
   CHECK(ps3_modes_dims(4)  == VIDEO_SCALE_PACK( 720,  480), "id 4");
   CHECK(ps3_modes_dims(5)  == VIDEO_SCALE_PACK( 720,  576), "id 5");
   CHECK(ps3_modes_dims(10) == VIDEO_SCALE_PACK(1600, 1080), "id 10");
   CHECK(ps3_modes_dims(11) == VIDEO_SCALE_PACK(1440, 1080), "id 11");
   CHECK(ps3_modes_dims(12) == VIDEO_SCALE_PACK(1280, 1080), "id 12");
   CHECK(ps3_modes_dims(13) == VIDEO_SCALE_PACK( 960, 1080), "id 13");
   CHECK(ps3_modes_dims(0) == 0 && ps3_modes_dims(3) == 0
         && ps3_modes_dims(0x81) == 0, "unknown ids have dims");
   CHECK(ps3_modes_hz(5) == 50.0f && fabsf(ps3_modes_hz(1) - 59.94f) < 0.001f,
         "rates");
}

static void test_display(void *data, const display_t *d)
{
   unsigned s, i, n;

   available_mask = d->mask;

   /* The system menu in each mode the display takes */
   for (s = 0; s < sizeof(all_ids) / sizeof(all_ids[0]); s++)
   {
      video_display_config_t *list;
      if (!(d->mask & BIT(all_ids[s])))
         continue;
      system_id = all_ids[s];
      g_global.console.screen.resolutions.current.id = 0;

      CHECK(ps3_display_server_resolution(system_id) == system_id,
            "%s: no choice does not give the system mode", d->name);

      list = get_list(data, &n);
      CHECK(list && n == d->count, "%s: %u entries, want %u",
            d->name, n, d->count);
      if (!list)
         continue;

      for (i = 0; i < n; i++)
      {
         CHECK(d->mask & BIT(list[i].idx), "%s: lists id %u it lacks",
               d->name, list[i].idx);
         CHECK(list[i].current == (list[i].idx == system_id),
               "%s: id %u current=%d with system %u", d->name,
               list[i].idx, list[i].current, system_id);
      }

      for (i = 0; i < n; i++)
      {
         char label[64];
         unsigned dims, m, j, out = 0;
         float hz;
         bool in_use = (list[i].idx
               == ps3_display_server_resolution(system_id));
         video_display_config_t *after;

         menu_label(label, sizeof(label), &list[i]);
         CHECK(menu_parse(label, &dims, &hz), "unparsable '%s'", label);

         reinits = 0;
         CHECK(dispserv_ps3.set_resolution(data, dims, (int)floor(hz), hz,
                  0, 0, 0, 0), "%s: '%s' refused", d->name, label);
         CHECK(g_global.console.screen.resolutions.current.id
               == list[i].idx, "%s: '%s' stored %u, want %u", d->name,
               label, g_global.console.screen.resolutions.current.id,
               list[i].idx);
         CHECK(reinits == (in_use ? 0u : 1u),
               "%s: '%s' reinitialised %u times", d->name, label, reinits);
         CHECK(ps3_display_server_resolution(system_id) == list[i].idx,
               "%s: '%s' is not what the driver configures", d->name,
               label);
         CHECK(fabsf(dispserv_ps3.get_refresh_rate(data)
                  - list[i].refreshrate_float) < 0.0001f,
               "%s: '%s' reports another rate", d->name, label);
         dispserv_ps3.get_video_output_size(data, &out, NULL, 0);
         CHECK(out == list[i].dims, "%s: '%s' reports %ux%u", d->name,
               label, VIDEO_SCALE_W(out), VIDEO_SCALE_H(out));

         after = get_list(data, &m);
         if (after)
         {
            unsigned cur = 0;
            for (j = 0; j < m; j++)
               if (after[j].current)
                  cur++;
            CHECK(cur == 1 && after[i].current,
                  "%s: after '%s' the wrong entry is current",
                  d->name, label);
            free(after);
         }
      }

      free(list);
   }
}

static void test_fallbacks(void *data)
{
   unsigned dims = 0;
   available_mask = BIT(4) | BIT(2);
   system_id      = 2;

   /* A stored choice the display does not take (moved to another TV) */
   g_global.console.screen.resolutions.current.id = 1;
   CHECK(ps3_display_server_resolution(system_id) == 2,
         "unavailable choice not replaced by the system mode");
   dispserv_ps3.get_video_output_size(data, &dims, NULL, 0);
   CHECK(dims == VIDEO_SCALE_PACK(1280, 720), "reports the lost choice");

   /* A stored id that names no mode */
   g_global.console.screen.resolutions.current.id = 0x81;
   CHECK(ps3_display_server_resolution(system_id) == 2, "id 0x81 used");

   /* Output state unreadable: the list still marks nothing wrongly */
   g_global.console.screen.resolutions.current.id = 0;
   state_fails = 1;
   CHECK(ps3_display_server_resolution(0) == 0, "invented a mode");
   state_fails = 0;

   g_global.console.screen.resolutions.current.id = 4;
   reinits = 0;
   CHECK(!dispserv_ps3.set_resolution(data, 0, 50, 50.0f, 0, 0, 0, 0),
         "refresh-rate-only switch accepted");
   CHECK(!dispserv_ps3.set_resolution(data, VIDEO_SCALE_PACK(1920, 1080),
            59, 59.94f, 0, 0, 0, 0), "1080 accepted on a 720 display");
   CHECK(!dispserv_ps3.set_resolution(data, VIDEO_SCALE_PACK(640, 480),
            59, 59.94f, 0, 0, 0, 0), "640x480 accepted");
   CHECK(reinits == 0, "a refused switch reinitialised");
   CHECK(g_global.console.screen.resolutions.current.id == 4,
         "a refused switch changed the stored id");
}

int main(void)
{
   unsigned i;
   void *data = dispserv_ps3.init();

   CHECK(data != NULL, "init returned NULL");
   CHECK(dispserv_ps3.ident && !strcmp(dispserv_ps3.ident, "ps3"), "ident");

   test_table_ids();
   for (i = 0; i < sizeof(displays) / sizeof(displays[0]); i++)
      test_display(data, &displays[i]);
   test_fallbacks(data);

   dispserv_ps3.destroy(data);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] dispserv_ps3_test\n");
   return 0;
}

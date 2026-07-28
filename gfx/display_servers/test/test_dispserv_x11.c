/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_dispserv_x11.c).
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

/* Regression tests for the XRandR query handling in
 * gfx/display_servers/dispserv_x11.c.
 *
 * These cover a crash that took RetroArch down inside
 * video_display_server_init(), before anything was drawn, on any X
 * server that advertises RandR without a usable output:
 *
 *   #0 x11_display_server_get_screen_orientation ()
 *   #1 video_display_server_init ()
 *   #2 rarch_main ()
 *
 * and an out-of-bounds read in the same walks, which indexed the
 * screen's crtc array with the output's crtc count.
 *
 * The tests run against a stub X server (xrandr_stub.c) rather than a
 * real one, because the behaviour under test is what happens when the
 * RandR queries *fail*, and a working display is exactly the
 * environment that cannot produce that.  The stub also numbers the
 * screen's crtcs and the output's crtcs from different bases, so which
 * array the code walked is readable straight off the call log.
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/extensions/Xrandr.h>

#include "../../video_display_server.h"
#include "xrandr_stub.h"

#define SUITE_NAME "dispserv_x11"

extern const video_display_server_t dispserv_x11;

/* Defaults: everything succeeds, one connected output, screen and
 * output agreeing on a single crtc. */
static void cfg_default(xrandr_stub_cfg_t *cfg)
{
   memset(cfg, 0, sizeof(*cfg));
   cfg->screen_ncrtc      = 1;
   cfg->output_ncrtc      = 1;
   cfg->output_connection = RR_Connected;
}

static void *serv_init(void)
{
   return dispserv_x11.init();
}

static void serv_free(void *data)
{
   if (data)
      dispserv_x11.destroy(data);
}

/* --- the crash --- */

START_TEST (test_orientation_output_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_output_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   /* Pre-fix this dereferenced NULL->connection and took the process
    * with it. */
   ck_assert_int_eq((int)dispserv_x11.get_screen_orientation(data),
         (int)ORIENTATION_NORMAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_orientation_crtc_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_crtc_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   /* Pre-fix this dereferenced NULL->width. */
   ck_assert_int_eq((int)dispserv_x11.get_screen_orientation(data),
         (int)ORIENTATION_NORMAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_orientation_screen_resources_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_screen_resources = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   ck_assert_int_eq((int)dispserv_x11.get_screen_orientation(data),
         (int)ORIENTATION_NORMAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_orientation_screen_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_screen_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   ck_assert_int_eq((int)dispserv_x11.get_screen_orientation(data),
         (int)ORIENTATION_NORMAL);
   serv_free(data);

   /* XRRFreeScreenConfigInfo() is not NULL-tolerant; the cleanup path
    * has to test before freeing.  The stub counts a NULL free as a bad
    * one rather than quietly accepting it. */
   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

/* A disconnected output is the ordinary shape of "RandR present, no
 * usable output" and must simply yield the default orientation. */
START_TEST (test_orientation_output_disconnected)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.output_connection = RR_Disconnected;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   ck_assert_int_eq((int)dispserv_x11.get_screen_orientation(data),
         (int)ORIENTATION_NORMAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->get_crtc_info_calls, 0);
   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

/* --- the out-of-bounds walk --- */

/* info->ncrtc bounds info->crtcs, not screen->crtcs.  Give the output
 * four crtcs and the screen one: walking the screen's array reads
 * three elements past its end, and every id it comes back with is the
 * wrong one. */
START_TEST (test_orientation_walks_output_crtcs)
{
   xrandr_stub_cfg_t cfg;
   const xrandr_stub_log_t *log;
   void *data;
   int i;

   cfg_default(&cfg);
   cfg.screen_ncrtc = 1;
   cfg.output_ncrtc = 4;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.get_screen_orientation(data);
   serv_free(data);

   log = xrandr_stub_log();
   ck_assert_int_eq(log->get_crtc_info_calls, 4);
   for (i = 0; i < log->get_crtc_info_calls; i++)
      ck_assert_int_eq((int)log->queried_crtcs[i],
            XRANDR_STUB_OUTPUT_CRTC_BASE + i);

   ck_assert_int_eq(log->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

/* The set path compounded it: it queried one crtc and then handed a
 * different one to XRRSetCrtcConfig(), configuring a crtc from
 * another's geometry.  Whatever it queries, it must configure. */
START_TEST (test_set_orientation_configures_queried_crtcs)
{
   xrandr_stub_cfg_t cfg;
   const xrandr_stub_log_t *log;
   void *data;
   int i;

   cfg_default(&cfg);
   cfg.screen_ncrtc = 1;
   cfg.output_ncrtc = 4;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.set_screen_orientation(data, ORIENTATION_VERTICAL);
   serv_free(data);

   /* The walk disables a crtc and then reconfigures it, so there are
    * two set calls per queried crtc; what matters is that every id it
    * configures is one it actually described, i.e. one of the
    * output's, never a screen crtc id it never looked at. */
   log = xrandr_stub_log();
   ck_assert_int_gt(log->set_crtc_config_calls, 0);
   for (i = 0; i < log->set_crtc_config_calls; i++)
   {
      int j, found = 0;
      for (j = 0; j < log->get_crtc_info_calls; j++)
         if (log->configured_crtcs[i] == log->queried_crtcs[j])
            found = 1;
      ck_assert_msg(found,
            "configured crtc %d was never queried",
            (int)log->configured_crtcs[i]);
   }

   ck_assert_int_eq(log->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_set_orientation_output_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_output_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.set_screen_orientation(data, ORIENTATION_VERTICAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->set_crtc_config_calls, 0);
   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_set_orientation_screen_resources_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_screen_resources = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.set_screen_orientation(data, ORIENTATION_VERTICAL);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->set_crtc_config_calls, 0);
   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

/* --- set_resolution --- */

START_TEST (test_set_resolution_output_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_output_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.set_resolution(data, 1280, 720, 60, 60.0f, 0, 0, 0, 0);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

START_TEST (test_set_resolution_crtc_info_null)
{
   xrandr_stub_cfg_t cfg;
   void *data;

   cfg_default(&cfg);
   cfg.fail_crtc_info = 1;
   xrandr_stub_reset(&cfg);

   data = serv_init();
   dispserv_x11.set_resolution(data, 1280, 720, 60, 60.0f, 0, 0, 0, 0);
   serv_free(data);

   ck_assert_int_eq(xrandr_stub_log()->set_crtc_config_calls, 0);
   ck_assert_int_eq(xrandr_stub_log()->bad_free, 0);
   ck_assert(xrandr_stub_all_freed());
}
END_TEST

Suite *create_suite(void)
{
   Suite *s       = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");

   tcase_add_test(tc_core, test_orientation_output_info_null);
   tcase_add_test(tc_core, test_orientation_crtc_info_null);
   tcase_add_test(tc_core, test_orientation_screen_resources_null);
   tcase_add_test(tc_core, test_orientation_screen_info_null);
   tcase_add_test(tc_core, test_orientation_output_disconnected);
   tcase_add_test(tc_core, test_orientation_walks_output_crtcs);
   tcase_add_test(tc_core, test_set_orientation_configures_queried_crtcs);
   tcase_add_test(tc_core, test_set_orientation_output_info_null);
   tcase_add_test(tc_core, test_set_orientation_screen_resources_null);
   tcase_add_test(tc_core, test_set_resolution_output_info_null);
   tcase_add_test(tc_core, test_set_resolution_crtc_info_null);

   suite_add_tcase(s, tc_core);
   return s;
}

int main(void)
{
   int num_fail;
   Suite   *s  = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

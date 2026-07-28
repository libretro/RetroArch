/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xrandr_stub.h).
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

#ifndef __XRANDR_STUB_H
#define __XRANDR_STUB_H

/* Knobs and observations shared between the stub X server in
 * xrandr_stub.c and the tests that drive it.
 *
 * The stub replaces libXrandr and the handful of Xlib entry points
 * dispserv_x11.c reaches, so the tests run with no X server, no RandR
 * extension and no display of any kind.  That is the point: the
 * behaviour under test is what happens when those queries *fail*, and
 * a working X server is precisely the environment that cannot produce
 * it.
 */

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

/* Knobs: set before calling into dispserv_x11. */
typedef struct
{
   /* Make the corresponding query hand back NULL. */
   int fail_open_display;
   int fail_screen_resources;
   int fail_screen_info;
   int fail_output_info;
   int fail_crtc_info;

   /* Number of crtcs the *screen* advertises, and the number the
    * *output* advertises.  Making the output's larger is what
    * distinguishes indexing info->crtcs from indexing screen->crtcs:
    * the latter runs off the end of the screen's array. */
   int screen_ncrtc;
   int output_ncrtc;

   /* RR_Connected or RR_Disconnected. */
   int output_connection;
} xrandr_stub_cfg_t;

/* Observations: read after. */
typedef struct
{
   int get_crtc_info_calls;
   int set_crtc_config_calls;
   int free_crtc_info_calls;
   int free_output_info_calls;
   int free_screen_resources_calls;
   int free_screen_config_calls;

   /* Every crtc id passed to XRRGetCrtcInfo()/XRRSetCrtcConfig().
    * The stub numbers the screen's crtcs from XRANDR_STUB_SCREEN_CRTC_BASE
    * and the output's from XRANDR_STUB_OUTPUT_CRTC_BASE, so which array
    * the code walked is readable straight off these. */
   RRCrtc queried_crtcs[16];
   RRCrtc configured_crtcs[16];

   /* Set if the stub was asked to free a pointer it never handed out,
    * or to free the same one twice. */
   int bad_free;
} xrandr_stub_log_t;

#define XRANDR_STUB_SCREEN_CRTC_BASE 100
#define XRANDR_STUB_OUTPUT_CRTC_BASE 200

void xrandr_stub_reset(const xrandr_stub_cfg_t *cfg);
const xrandr_stub_log_t *xrandr_stub_log(void);

/* Non-zero if every pointer the stub handed out has been freed. */
int xrandr_stub_all_freed(void);

#endif

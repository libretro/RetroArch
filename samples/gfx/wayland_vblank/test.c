/* shipping.inc, cut by run.py: the connector code of an output's
 * name, the window's connector as told between threads, the tearing a
 * presentation feedback reports, the contexts' present_immediate and
 * the display server's use of the DRM clock. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>

typedef long retro_time_t;
typedef int retro_atomic_int_t;
#define retro_atomic_store_relaxed_int(p, v) (*(p) = (v))
#define retro_atomic_load_relaxed_int(p) (*(p))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define WP_PRESENTATION_FEEDBACK_KIND_VSYNC 1

struct wp_presentation_feedback { int unused; };
struct wl_output { int unused; };
typedef struct
{
   uint64_t last_ust;
   uint64_t refresh_interval;
   bool presented;
   bool tearing;
} wl_present_t;
static unsigned removed;
static void wl_present_remove(wl_present_t *present,
      struct wp_presentation_feedback *feedback)
{ (void)present; (void)feedback; removed++; }

typedef struct
{
   void *tearing_control;
   int swap_interval;
   bool suspended;
   wl_present_t present;
} gfx_ctx_wayland_data_t;

typedef struct { unsigned connector; } output_info_t;
static output_info_t *wl_window_output;

/* drm_vblank.h's, as the display server calls them */
typedef struct { int fd; unsigned crtc; bool queued; } drm_vblank_t;
static drm_vblank_t wl_vblank = { -1, 0, false };
static unsigned opens, open_code, closes, waits;
static bool open_fails;
static bool drm_vblank_open_connector(drm_vblank_t *vb, unsigned code)
{
   opens++;
   open_code = code;
   if (open_fails)
      return false;
   vb->fd = 40 + (int)opens;
   return true;
}
static unsigned drm_vblank_wait(drm_vblank_t *vb, int fd, unsigned seen,
      unsigned timeout_us, retro_time_t *at)
{
   assert(vb == &wl_vblank && fd == vb->fd && timeout_us == 5000);
   waits++;
   *at = 1234;
   return seen + 1;
}
static int close(int fd) { assert(fd > 40); closes++; return 0; }

#include "shipping.inc"

#define CODE(type, id) (((unsigned)(type) << 16) | (unsigned)(id))

int main(void)
{
   gfx_ctx_wayland_data_t wl;
   output_info_t out, other;
   struct wl_output wo;
   retro_time_t at = 0;
   int control     = 0;

   /* ---- a wl_output's name is a DRM connector's ---- */
   assert(gfx_ctx_wl_connector_code("DP-1")       == CODE(10, 1));
   assert(gfx_ctx_wl_connector_code("DP-12")      == CODE(10, 12));
   assert(gfx_ctx_wl_connector_code("HDMI-A-1")   == CODE(11, 1));
   assert(gfx_ctx_wl_connector_code("HDMI-B-2")   == CODE(12, 2));
   assert(gfx_ctx_wl_connector_code("eDP-1")      == CODE(14, 1));
   assert(gfx_ctx_wl_connector_code("DVI-D-1")    == CODE(3, 1));
   assert(gfx_ctx_wl_connector_code("Virtual-1")  == CODE(15, 1));
   assert(gfx_ctx_wl_connector_code("VGA-1")      == CODE(1, 1));
   assert(gfx_ctx_wl_connector_code("USB-3")      == CODE(20, 3));
   /* Mutter's, for either kind of HDMI */
   assert(gfx_ctx_wl_connector_code("HDMI-1")     == CODE(0xff, 1));
   /* not one */
   assert(!gfx_ctx_wl_connector_code(NULL));
   assert(!gfx_ctx_wl_connector_code(""));
   assert(!gfx_ctx_wl_connector_code("DP"));
   assert(!gfx_ctx_wl_connector_code("DP-"));
   assert(!gfx_ctx_wl_connector_code("-1"));
   assert(!gfx_ctx_wl_connector_code("DP-0"));
   assert(!gfx_ctx_wl_connector_code("DP-1a"));
   assert(!gfx_ctx_wl_connector_code("DP-99999999999"));
   assert(!gfx_ctx_wl_connector_code("WL-1"));
   assert(!gfx_ctx_wl_connector_code("HEADLESS-1"));
   assert(!gfx_ctx_wl_connector_code("Unknown-1"));
   assert(!gfx_ctx_wl_connector_code("dp-1"));
   assert(!gfx_ctx_wl_connector_code("DPX-1"));

   /* ---- named after the window is on it: told then ---- */
   out.connector    = 0;
   other.connector  = 0;
   wl_window_output = &out;
   wl_output_handle_name(&other, &wo, "DP-2");
   assert(other.connector == CODE(10, 2) && !gfx_ctx_wl_window_connector());
   wl_output_handle_name(&out, &wo, "HDMI-A-1");
   assert(out.connector == CODE(11, 1));
   assert(gfx_ctx_wl_window_connector() == CODE(11, 1));

   /* ---- the display server's clock follows the window's connector ---- */
   assert(wl_display_server_wait_vblank(NULL, 7, 5000, &at) == 8 && at == 1234);
   assert(opens == 1 && open_code == CODE(11, 1) && waits == 1 && !closes);
   assert(wl_display_server_wait_vblank(NULL, 8, 5000, &at) == 9);
   assert(opens == 1 && waits == 2);
   /* on another output: that one's, the last one closed */
   gfx_ctx_wl_window_connector_set(CODE(10, 2));
   assert(wl_display_server_wait_vblank(NULL, 9, 5000, &at) == 10);
   assert(opens == 2 && open_code == CODE(10, 2) && closes == 1);
   /* one DRM has nothing for is asked about once, not every frame */
   open_fails = true;
   gfx_ctx_wl_window_connector_set(CODE(14, 1));
   assert(!wl_display_server_wait_vblank(NULL, 10, 5000, &at));
   assert(!wl_display_server_wait_vblank(NULL, 10, 5000, &at));
   assert(opens == 3 && closes == 2 && waits == 3);
   /* no named output: no clock, and nothing asked */
   open_fails = false;
   gfx_ctx_wl_window_connector_set(0);
   assert(!wl_display_server_wait_vblank(NULL, 10, 5000, &at));
   assert(opens == 3 && closes == 2);
   /* back on one */
   gfx_ctx_wl_window_connector_set(CODE(11, 1));
   assert(wl_display_server_wait_vblank(NULL, 10, 5000, &at) == 11);
   assert(opens == 4);
   wl_vblank_close();
   assert(closes == 3 && wl_vblank.fd < 0);

   /* ---- a feedback says whether its frame was held for a vblank ---- */
   memset(&wl, 0, sizeof(wl));
   wl_present_feedback_presented(&wl.present, NULL, 0, 5, 600, 16666666,
         0, 9, WP_PRESENTATION_FEEDBACK_KIND_VSYNC | 4);
   assert(wl.present.presented && !wl.present.tearing && removed == 1);
   assert(wl.present.refresh_interval == 16666666);
   wl_present_feedback_presented(&wl.present, NULL, 0, 5, 700, 16666666,
         0, 9, 4);
   assert(wl.present.tearing);

   /* ---- and a present tears where it was let: asked for, with no
    * swap interval, shown, and said to have ---- */
   assert(!gfx_ctx_wl_present_immediate_common(NULL));
   assert(!gfx_ctx_wl_present_immediate_common(&wl));   /* never asked */
   wl.tearing_control = &control;
   assert(gfx_ctx_wl_present_immediate_common(&wl));
   wl.swap_interval = 1;
   assert(!gfx_ctx_wl_present_immediate_common(&wl));
   wl.swap_interval = 0;
   wl.suspended     = true;
   assert(!gfx_ctx_wl_present_immediate_common(&wl));
   wl.suspended     = false;
   /* the compositor holding it all the same */
   wl_present_feedback_presented(&wl.present, NULL, 0, 5, 800, 16666666,
         0, 9, WP_PRESENTATION_FEEDBACK_KIND_VSYNC);
   assert(!gfx_ctx_wl_present_immediate_common(&wl));
   puts("PASS Wayland vblank pacing: connector names, the clock's connector and tearing feedback");
   return 0;
}

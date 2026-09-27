/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x11_input_poll_test.c).
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
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The X input driver's poll runs every frame and must not wait on the
 * server: the test requires it to send no requests on either
 * connection, and drives the keyboard and pointer through XTEST to
 * check that they still read as they did - keys across auto-repeat,
 * a change of focus and a keyboard grab by another client; buttons,
 * the wheel, a drag out of the window, and the grabbed pointer's
 * motion across a warp the server has not yet made. Needs an X server
 * without a window manager (Xvfb) with XTEST and XInput 2. */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/XInput.h>

#include "../../../input/drivers/x11_input.c"
#include "../../../gfx/common/x11_common.h"

#define TEST_MASK (StructureNotifyMask | KeyPressMask | KeyReleaseMask \
      | LeaveWindowMask | EnterWindowMask | ButtonReleaseMask \
      | ButtonPressMask | FocusChangeMask)

/* Stubs for what the two files reach outside the keyboard poll. */
static int stub_signal_state;
const struct rarch_key_map rarch_key_map_x11[] = { { 0, RETROK_UNKNOWN } };
enum retro_key rarch_keysym_lut[RETROK_LAST];
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}
int frontend_driver_get_signal_handler_state(void) { return stub_signal_state; }
void frontend_driver_set_signal_handler_state(int v) { stub_signal_state = v; }
void frontend_driver_destroy_signal_handler_state(void) { stub_signal_state = 0; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
void input_config_set_mouse_display_name(unsigned port, const char *name)
{ (void)port; (void)name; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ (void)x; (void)y; return true; }
bool video_driver_get_viewport_info(struct video_viewport *viewport)
{ (void)viewport; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, int16_t *res_x, int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y, bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_x; (void)res_y;
   (void)res_screen_x; (void)res_screen_y; (void)report_oob;
   return false;
}
linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate)
{ (void)rate; return NULL; }
void linux_close_illuminance_sensor(linux_illuminance_sensor_t *sensor)
{ (void)sensor; }
float linux_get_illuminance_reading(const linux_illuminance_sensor_t *sensor)
{ (void)sensor; return 0.0f; }
void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *sensor,
      unsigned rate)
{ (void)sensor; (void)rate; }
/* Every mouse port reads the first master pointer. */
static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
bool video_driver_has_focus(void) { return x11_has_focus(NULL); }
enum rarch_display_type video_driver_display_type_get(void)
{ return RARCH_DISPLAY_X11; }
uintptr_t video_driver_display_get(void) { return (uintptr_t)g_x11_dpy; }
uintptr_t video_driver_window_get(void) { return (uintptr_t)g_x11_win; }
void video_driver_display_type_set(enum rarch_display_type type) { (void)type; }
void video_driver_display_set(uintptr_t idx) { (void)idx; }
void video_driver_window_set(uintptr_t idx) { (void)idx; }
size_t video_driver_get_window_title(char *s, size_t len)
{ (void)len; s[0] = '\0'; return 0; }

static int failures;
static Display *inj;
static KeyCode kc_a;

#define CHECK(cond, what) do { \
   if (!(cond)) { \
      fprintf(stderr, "[FAIL] %s (line %d)\n", what, __LINE__); \
      failures++; \
   } else \
      printf("[ok] %s\n", what); \
} while (0)

/* XTEST presses repeat by the controls of the XTEST keyboard device,
 * not the core keyboard's. Off except where a lane wants it, so no
 * other lane can pass on a repeat. */
static void set_repeat(bool on)
{
   int n, i;
   XDeviceInfo *di = XListInputDevices(inj, &n);
   for (i = 0; di && i < n; i++)
      if (strstr(di[i].name, "XTEST keyboard"))
      {
         XkbChangeEnabledControls(inj, (unsigned)di[i].id,
               XkbRepeatKeysMask, on ? XkbRepeatKeysMask : 0);
         XkbSetAutoRepeatRate(inj, (unsigned)di[i].id, 60, 20);
      }
   if (di)
      XFreeDeviceList(di);
   if (on)
      XAutoRepeatOn(inj);
   else
      XAutoRepeatOff(inj);
   XSync(inj, False);
}

static void nap_ms(long ms)
{
   struct timespec ts;
   ts.tv_sec  = ms / 1000;
   ts.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}

/* One frame: the video side pumps the shared connection, then the
 * input driver polls. */
static void frame(x11_input_t *x11)
{
   bool quit     = false;
   bool resize   = false;
   unsigned dims = 0;
   x11_check_window(NULL, &quit, &resize, &dims);
   input_x.poll(x11);
}

static bool held(x11_input_t *x11)
{
   return x_keyboard_pressed(x11, RETROK_a);
}

/* Events reach the connections asynchronously; give them up to 2 s. */
static bool settles_to(x11_input_t *x11, bool want)
{
   unsigned i;
   XSync(inj, False);
   for (i = 0; i < 400; i++)
   {
      frame(x11);
      if (held(x11) == want)
         return true;
      nap_ms(5);
   }
   return false;
}

static void key(bool down)
{
   XTestFakeKeyEvent(inj, kc_a, down ? True : False, CurrentTime);
   XSync(inj, False);
}

static void focus(Window w)
{
   XSetInputFocus(inj, w, RevertToParent, CurrentTime);
   XSync(inj, False);
}

static void move_to(int x, int y)
{
   XWarpPointer(inj, None, DefaultRootWindow(inj), 0, 0, 0, 0, x, y);
   XSync(inj, False);
}

static void button(unsigned b, bool down)
{
   XTestFakeButtonEvent(inj, b, down ? True : False, CurrentTime);
   XSync(inj, False);
}

static int want_x, want_y;
static bool at_want(x11_input_t *x11)
{ return x11->mouse_x[0] == want_x && x11->mouse_y[0] == want_y; }
static bool left_held(x11_input_t *x11) { return x11->mouse_l[0]; }
static bool left_free(x11_input_t *x11) { return !x11->mouse_l[0]; }
static bool right_held(x11_input_t *x11) { return x11->mouse_r[0]; }
static bool right_free(x11_input_t *x11) { return !x11->mouse_r[0]; }
static bool b8_held(x11_input_t *x11) { return x11->mouse_4[0]; }
static bool b8_free(x11_input_t *x11) { return !x11->mouse_4[0]; }
static bool wheel_up(x11_input_t *x11)
{ (void)x11; return x_mouse_state_wheel(RETRO_DEVICE_ID_MOUSE_WHEELUP) != 0; }

static bool until(x11_input_t *x11, bool (*cond)(x11_input_t*))
{
   unsigned i;
   for (i = 0; i < 400; i++)
   {
      frame(x11);
      if (cond(x11))
         return true;
      nap_ms(5);
   }
   return false;
}

/* Frames until the grabbed deltas add up to (dx, dy), then 20 more
 * that must add nothing. */
static bool grabbed_sums_to(x11_input_t *x11, int dx, int dy,
      int *sx, int *sy)
{
   unsigned i;
   for (i = 0; i < 400 && (*sx != dx || *sy != dy); i++)
   {
      frame(x11);
      *sx += x11->mouse_delta_x[0];
      *sy += x11->mouse_delta_y[0];
      nap_ms(5);
   }
   for (i = 0; i < 20; i++)
   {
      frame(x11);
      *sx += x11->mouse_delta_x[0];
      *sy += x11->mouse_delta_y[0];
      nap_ms(2);
   }
   return *sx == dx && *sy == dy;
}

static Bool is_key_release(Display *dpy, XEvent *ev, XPointer arg)
{
   (void)dpy; (void)arg;
   return ev->type == KeyRelease;
}

int main(void)
{
   unsigned i;
   unsigned long before;
   unsigned long key_before;
   int ev_base, err_base, major, minor;
   XSetWindowAttributes swa;
   XEvent ev;
   Window root;
   Window watch_win;
   Display *watch;
   unsigned repeats_seen = 0;
   x11_input_t *x11;

   if (!x11_connect())
   {
      fprintf(stderr, "[FAIL] no X display (DISPLAY=%s)\n",
            getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
      return 1;
   }
   if (!(inj = XOpenDisplay(DisplayString(g_x11_dpy)))
         || !XTestQueryExtension(inj, &ev_base, &err_base, &major, &minor))
   {
      fprintf(stderr, "[FAIL] no XTEST on %s\n", DisplayString(g_x11_dpy));
      return 1;
   }
   root  = DefaultRootWindow(g_x11_dpy);
   kc_a  = XKeysymToKeycode(inj, XK_a);
   rarch_keysym_lut[RETROK_a] = kc_a;

   /* Keep the pointer off the window: the poll reads it only inside. */
   XWarpPointer(inj, None, root, 0, 0, 0, 0, 1000, 700);
   set_repeat(false);

   swa.event_mask        = TEST_MASK;
   swa.override_redirect = False;
   g_x11_win = XCreateWindow(g_x11_dpy, root, 0, 0, 320, 240, 0,
         CopyFromParent, InputOutput, CopyFromParent,
         CWEventMask | CWOverrideRedirect, &swa);
   XMapWindow(g_x11_dpy, g_x11_win);
   x11_event_queue_check(&ev);
   if (!x11_input_ctx_new(false))
      fprintf(stderr, "[note] no input method; key lanes unaffected\n");
   focus(g_x11_win);

   /* A key already down when the driver starts reads as held. */
   key(true);
   x11 = (x11_input_t*)input_x.init(NULL);
   CHECK(x11 && x11->event_display, "driver opens its key connection");
   if (!x11 || !x11->event_display)
      return 1;
   CHECK(settles_to(x11, true), "key held before init reads held");
   key(false);
   CHECK(settles_to(x11, false), "release reads released");

   /* The poll sends nothing on the shared connection or its own. */
   key(true);
   CHECK(settles_to(x11, true), "press reads held");
   XSync(g_x11_dpy, False);
   before     = NextRequest(g_x11_dpy);
   key_before = NextRequest(x11->event_display);
   for (i = 0; i < 200; i++)
      frame(x11);
   printf("[info] 200 frames sent %lu + %lu requests\n",
         NextRequest(g_x11_dpy) - before,
         NextRequest(x11->event_display) - key_before);
   CHECK(NextRequest(g_x11_dpy) == before,
         "200 polls send no requests on the shared connection");
   CHECK(NextRequest(x11->event_display) == key_before,
         "200 polls send no requests on the key connection");
   CHECK(held(x11), "key still reads held after 200 polls");
   key(false);
   CHECK(settles_to(x11, false), "release reads released");

   /* Auto-repeat. A client without detectable auto-repeat, watching
    * the same window, shows the server is repeating at all; the
    * server sends it a release only with a press it selected. The
    * key connection is left undrained over the hold, and must hold no
    * release for the key it has. */
   watch = XOpenDisplay(DisplayString(g_x11_dpy));
   watch_win = g_x11_win;
   XSelectInput(watch, watch_win, KeyPressMask | KeyReleaseMask);
   XSync(watch, False);
   set_repeat(true);
   key(true);
   CHECK(settles_to(x11, true), "press reads held");
   nap_ms(400);
   XSync(x11->event_display, False);
   CHECK(!XCheckIfEvent(x11->event_display, &ev, is_key_release, NULL),
         "held through auto-repeat, the key connection gets no release");
   for (i = 0; i < 20; i++)
   {
      frame(x11);
      nap_ms(5);
   }
   CHECK(held(x11), "held through auto-repeat reads held");
   key(false);
   CHECK(settles_to(x11, false), "release reads released");
   set_repeat(false);
   while (XPending(watch))
   {
      XNextEvent(watch, &ev);
      if (ev.type == KeyRelease)
         repeats_seen++;
   }
   printf("[info] a plain client saw %u releases for one press\n",
         repeats_seen);
   CHECK(repeats_seen > 1, "the server auto-repeats (lane is live)");
   XCloseDisplay(watch);

   /* Released while focus is elsewhere: reads released on return. */
   key(true);
   CHECK(settles_to(x11, true), "press reads held");
   focus(root);
   CHECK(settles_to(x11, false), "unfocused reads released");
   key(false);
   focus(g_x11_win);
   CHECK(settles_to(x11, false), "released while away reads released");
   for (i = 0; i < 20; i++)
      frame(x11);
   CHECK(!held(x11), "and stays released");

   /* Pressed while focus is elsewhere: reads held on return. */
   focus(root);
   settles_to(x11, false);
   key(true);
   focus(g_x11_win);
   CHECK(settles_to(x11, true), "pressed while away reads held on return");
   key(false);
   CHECK(settles_to(x11, false), "release reads released");

   /* Another client's keyboard grab keeps the window's focus, and a
    * key held into it stays held; one released under it reads
    * released once the grab ends. */
   key(true);
   CHECK(settles_to(x11, true), "press reads held");
   XGrabKeyboard(inj, root, False, GrabModeAsync, GrabModeAsync,
         CurrentTime);
   XSync(inj, False);
   for (i = 0; i < 20; i++)
   {
      frame(x11);
      nap_ms(2);
   }
   CHECK(held(x11), "held into a grab stays held");
   key(false);
   XUngrabKeyboard(inj, CurrentTime);
   CHECK(settles_to(x11, false), "released during a grab reads released");
   for (i = 0; i < 20; i++)
      frame(x11);
   CHECK(!held(x11), "and stays released");

   /* The pointer. */
   CHECK(x11->ptr_events, "the pointer comes from events");
   move_to(1000, 700);
   frame(x11);
   want_x = 100; want_y = 100;
   move_to(100, 100);
   CHECK(until(x11, at_want), "motion into the window reads its position");

   XSync(g_x11_dpy, False);
   before     = NextRequest(g_x11_dpy);
   key_before = NextRequest(x11->event_display);
   for (i = 0; i < 200; i++)
   {
      if (i % 10 == 0)
         move_to(100 + (int)i / 10, 100);
      frame(x11);
   }
   printf("[info] 200 frames with the pointer moving sent %lu + %lu requests\n",
         NextRequest(g_x11_dpy) - before,
         NextRequest(x11->event_display) - key_before);
   CHECK(NextRequest(g_x11_dpy) == before
         && NextRequest(x11->event_display) == key_before,
         "200 polls with the pointer inside send no requests");
   want_x = 119;
   CHECK(until(x11, at_want), "and read where it went");

   button(1, true);
   CHECK(until(x11, left_held), "press without motion reads held");
   button(1, false);
   CHECK(until(x11, left_free), "release reads released");
   button(3, true);
   CHECK(until(x11, right_held), "right button reads held");
   button(3, false);
   CHECK(until(x11, right_free), "and released");
   button(8, true);
   CHECK(until(x11, b8_held), "button 8 reads as mouse button 4");
   button(8, false);
   CHECK(until(x11, b8_free), "and released");
   button(4, true);
   button(4, false);
   CHECK(until(x11, wheel_up), "a wheel notch is latched");
   frame(x11);
   CHECK(!wheel_up(x11), "once");

   /* Out of the window a button reads released, dragged or not. */
   button(1, true);
   CHECK(until(x11, left_held), "press reads held");
   move_to(500, 500);
   CHECK(until(x11, left_free), "dragged out of the window reads released");
   move_to(50, 50);
   CHECK(until(x11, left_held), "back in while held reads held");
   move_to(500, 500);
   CHECK(until(x11, left_free), "out again reads released");
   button(1, false);
   want_x = 100; want_y = 100;
   move_to(100, 100);
   CHECK(until(x11, at_want) && !x11->mouse_l[0],
         "released outside reads released on return");

   /* Grabbed: deltas from the centre, the pointer warped back. */
   {
      int sx = 0, sy = 0;
      want_x = 60; want_y = 60;
      move_to(60, 60);
      CHECK(until(x11, at_want), "pointer at (60,60)");
      x_grab_mouse(x11, true);
      CHECK(grabbed_sums_to(x11, 60 - 160, 60 - 120, &sx, &sy),
            "grabbed: the first poll reads the offset from the centre");

      XSync(x11->event_display, False);
      XSync(g_x11_dpy, False);
      before     = NextRequest(g_x11_dpy);
      key_before = NextRequest(x11->event_display);
      for (i = 0; i < 100; i++)
         frame(x11);
      CHECK(NextRequest(g_x11_dpy) == before
            && NextRequest(x11->event_display) == key_before,
            "grabbed and still: 100 polls send no requests");

      sx = sy = 0;
      XTestFakeRelativeMotionEvent(inj, 7, 3, CurrentTime);
      XSync(inj, False);
      CHECK(grabbed_sums_to(x11, 7, 3, &sx, &sy),
            "grabbed motion reads as the motion made");

      /* The server holds the warp back: motion it reports from before
       * the warp is what the warp undoes, and is not read again. With
       * the server grabbed any wait on it would hang here. */
      sx = sy = 0;
      XGrabServer(inj);
      XTestFakeRelativeMotionEvent(inj, 20, 0, CurrentTime);
      XSync(inj, False);
      CHECK(grabbed_sums_to(x11, 20, 0, &sx, &sy),
            "motion before a warp reads once");
      XTestFakeRelativeMotionEvent(inj, 10, 0, CurrentTime);
      XSync(inj, False);
      nap_ms(20);
      for (i = 0; i < 20; i++)
      {
         frame(x11);
         sx += x11->mouse_delta_x[0];
         sy += x11->mouse_delta_y[0];
         nap_ms(2);
      }
      XUngrabServer(inj);
      XSync(inj, False);
      for (i = 0; i < 50; i++)
      {
         frame(x11);
         sx += x11->mouse_delta_x[0];
         sy += x11->mouse_delta_y[0];
         nap_ms(2);
      }
      printf("[info] held-back warp: deltas summed to (%d,%d)\n", sx, sy);
      CHECK(sx == 20 && sy == 0,
            "motion the warp replaces is not read twice");
      x_grab_mouse(x11, false);
   }

   /* A second driver on the window: a server that keeps XI2 button
    * presses to one client refuses it and it asks the server
    * instead; one that does not gives it the events too. Either way
    * the process lives and the pointer reads. */
   {
      x11_input_t *second = (x11_input_t*)input_x.init(NULL);
      printf("[info] second driver reads the pointer %s\n",
            second && second->ptr_events ? "from events" : "by asking");
      want_x = 110; want_y = 100;
      move_to(110, 100);
      CHECK(second && until(second, at_want),
            "a second driver on the window reads the pointer");
      input_x.free(second);
   }

   /* A pointer already in the window, a button held, reads as such on
    * the first poll of a new driver. */
   {
      x11_input_t *fresh;
      move_to(80, 90);
      button(1, true);
      input_x.free(x11);
      fresh = (x11_input_t*)input_x.init(NULL);
      CHECK(fresh && fresh->ptr_events, "a new driver reads events");
      input_x.poll(fresh);
      CHECK(fresh->mouse_x[0] == 80 && fresh->mouse_y[0] == 90
            && fresh->mouse_l[0],
            "the first poll reads where the pointer is and what is held");
      button(1, false);
      x11 = fresh;
   }

   /* Without its own connection the driver asks the server. */
   XCloseDisplay(x11->event_display);
   x11->event_display = NULL;
   x11->ptr_events    = false;
   key(true);
   CHECK(settles_to(x11, true), "fallback: press reads held");
   key(false);
   CHECK(settles_to(x11, false), "fallback: release reads released");

   input_x.free(x11);
   x11_input_ctx_destroy();
   x11_window_destroy(false);
   XCloseDisplay(inj);
   XCloseDisplay(g_x11_dpy);
   g_x11_dpy = NULL;

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}

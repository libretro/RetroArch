/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x11_key_poll_test.c).
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
 * connection, and drives the keyboard through XTEST to check that
 * keys still read as held and released - across auto-repeat, a
 * change of focus and a keyboard grab by another client. Needs an X
 * server without a window manager (Xvfb) with the XTEST extension. */

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
void x_input_poll_wheel(XButtonEvent *event, bool latch)
{ (void)event; (void)latch; }
int16_t x_mouse_state_wheel(unsigned id) { (void)id; return 0; }
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
/* Only reached for pointer reads, which the test does not make. */
settings_t *config_get_ptr(void) { return NULL; }
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
   CHECK(x11 && x11->key_display, "driver opens its key connection");
   if (!x11 || !x11->key_display)
      return 1;
   CHECK(settles_to(x11, true), "key held before init reads held");
   key(false);
   CHECK(settles_to(x11, false), "release reads released");

   /* The poll sends nothing on the shared connection or its own. */
   key(true);
   CHECK(settles_to(x11, true), "press reads held");
   XSync(g_x11_dpy, False);
   before     = NextRequest(g_x11_dpy);
   key_before = NextRequest(x11->key_display);
   for (i = 0; i < 200; i++)
      frame(x11);
   printf("[info] 200 frames sent %lu + %lu requests\n",
         NextRequest(g_x11_dpy) - before,
         NextRequest(x11->key_display) - key_before);
   CHECK(NextRequest(g_x11_dpy) == before,
         "200 polls send no requests on the shared connection");
   CHECK(NextRequest(x11->key_display) == key_before,
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
   XSync(x11->key_display, False);
   CHECK(!XCheckIfEvent(x11->key_display, &ev, is_key_release, NULL),
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

   /* Without its own connection the driver asks the server. */
   XCloseDisplay(x11->key_display);
   x11->key_display = NULL;
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

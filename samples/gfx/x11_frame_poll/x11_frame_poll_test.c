/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x11_frame_poll_test.c).
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

/* x11_check_window() and x11_has_focus() run every frame and must not
 * wait on the server: the test requires them to send no requests, and
 * checks size and focus still follow the window. Needs an X server
 * without a window manager (Xvfb). The window uses the X context
 * drivers' event mask. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <boolean.h>

#include "../../../gfx/common/x11_common.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_keymaps.h"

#define TEST_MASK (StructureNotifyMask | KeyPressMask | KeyReleaseMask \
      | LeaveWindowMask | EnterWindowMask | ButtonReleaseMask \
      | ButtonPressMask | FocusChangeMask)

/* Stubs for what x11_common.c reaches outside the window poll. */
static int stub_signal_state;
const struct rarch_key_map rarch_key_map_x11[] = { { 0, RETROK_UNKNOWN } };
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
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
enum rarch_display_type video_driver_display_type_get(void)
{ return RARCH_DISPLAY_X11; }
uintptr_t video_driver_window_get(void) { return (uintptr_t)g_x11_win; }
void video_driver_display_type_set(enum rarch_display_type type) { (void)type; }
void video_driver_display_set(uintptr_t idx) { (void)idx; }
void video_driver_window_set(uintptr_t idx) { (void)idx; }
size_t video_driver_get_window_title(char *s, size_t len)
{ (void)len; s[0] = '\0'; return 0; }

static int failures;

#define CHECK(cond, what) do { \
   if (!(cond)) { \
      fprintf(stderr, "[FAIL] %s (line %d)\n", what, __LINE__); \
      failures++; \
   } else \
      printf("[ok] %s\n", what); \
} while (0)

static void open_window(unsigned w, unsigned h)
{
   XEvent ev;
   XSetWindowAttributes swa;
   Window root = DefaultRootWindow(g_x11_dpy);
   swa.event_mask        = TEST_MASK;
   swa.override_redirect = False;
   g_x11_win = XCreateWindow(g_x11_dpy, root, 0, 0, w, h, 0,
         CopyFromParent, InputOutput, CopyFromParent,
         CWEventMask | CWOverrideRedirect, &swa);
   XMapWindow(g_x11_dpy, g_x11_win);
   x11_event_queue_check(&ev);
   if (!x11_input_ctx_new(false))
      fprintf(stderr, "[note] no input method; focus lanes unaffected\n");
}

static unsigned poll_frame(unsigned *dims, bool *resize, bool *focus)
{
   bool quit = false;
   *resize   = false;
   x11_check_window(NULL, &quit, resize, dims);
   *focus    = x11_has_focus(NULL);
   return *dims;
}

static void settle(unsigned *dims, bool *resize, bool *focus)
{
   XSync(g_x11_dpy, False);
   poll_frame(dims, resize, focus);
}

int main(void)
{
   unsigned i;
   unsigned long before;
   unsigned dims = 0;
   bool resize   = false;
   bool focus    = false;
   Window root;

   if (!x11_connect())
   {
      fprintf(stderr, "[FAIL] no X display (DISPLAY=%s)\n",
            getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
      return 1;
   }
   root = DefaultRootWindow(g_x11_dpy);
   XSetInputFocus(g_x11_dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

   open_window(320, 240);
   settle(&dims, &resize, &focus);
   CHECK(VIDEO_SCALE_W(dims) == 320 && VIDEO_SCALE_H(dims) == 240,
         "size is the created size without a ConfigureNotify");

   XSync(g_x11_dpy, False);
   before = NextRequest(g_x11_dpy);
   for (i = 0; i < 200; i++)
      poll_frame(&dims, &resize, &focus);
   printf("[info] 200 frames sent %lu requests\n",
         NextRequest(g_x11_dpy) - before);
   CHECK(NextRequest(g_x11_dpy) == before,
         "200 frames of check_window + has_focus send no requests");

   /* The poll reads the size before it pumps events, so a resize
    * can show on either of the next two frames. */
   XResizeWindow(g_x11_dpy, g_x11_win, 400, 300);
   settle(&dims, &resize, &focus);
   {
      bool first = resize;
      poll_frame(&dims, &resize, &focus);
      resize = resize || first;
   }
   CHECK(resize && VIDEO_SCALE_W(dims) == 400 && VIDEO_SCALE_H(dims) == 300,
         "resize reported from ConfigureNotify");

   /* Focus follows FocusIn/FocusOut on the window itself. */
   CHECK(!focus, "PointerRoot focus is not window focus");
   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToParent, CurrentTime);
   settle(&dims, &resize, &focus);
   CHECK(focus, "focused after XSetInputFocus(window)");

   XGrabKeyboard(g_x11_dpy, root, False, GrabModeAsync, GrabModeAsync,
         CurrentTime);
   settle(&dims, &resize, &focus);
   CHECK(focus, "focus kept across a keyboard grab");
   XUngrabKeyboard(g_x11_dpy, CurrentTime);
   settle(&dims, &resize, &focus);
   CHECK(focus, "focus kept across the ungrab");

   XSetInputFocus(g_x11_dpy, root, RevertToParent, CurrentTime);
   settle(&dims, &resize, &focus);
   CHECK(!focus, "unfocused when focus moves to the root window");

   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToParent, CurrentTime);
   settle(&dims, &resize, &focus);
   XSetInputFocus(g_x11_dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
   settle(&dims, &resize, &focus);
   CHECK(!focus, "unfocused when focus moves to PointerRoot");

   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToParent, CurrentTime);
   settle(&dims, &resize, &focus);
   XUnmapWindow(g_x11_dpy, g_x11_win);
   settle(&dims, &resize, &focus);
   CHECK(!focus, "unfocused once unmapped");

   /* A new window starts from its own size and no focus. */
   x11_input_ctx_destroy();
   x11_window_destroy(false);
   XSetInputFocus(g_x11_dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
   open_window(640, 480);
   settle(&dims, &resize, &focus);
   CHECK(VIDEO_SCALE_W(dims) == 640 && VIDEO_SCALE_H(dims) == 480,
         "second window reports its own size");
   CHECK(!focus, "second window starts unfocused");

   x11_input_ctx_destroy();
   x11_window_destroy(false);
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

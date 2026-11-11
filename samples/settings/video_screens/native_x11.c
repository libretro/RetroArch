/* The X11 contexts' native-window lifecycle for video_screens_test.c,
 * #included there. */
#ifdef HAVE_X11
typedef struct
{
   int x;
   int y;
   unsigned w;
   unsigned h;
} native_rect_t;

static bool native_x11_ident(const char *ident)
{
   return    strcmp(ident, "x") == 0
          || strcmp(ident, "egl_x") == 0
          || strcmp(ident, "vk_x") == 0;
}

/* The window's id, which a Vulkan context's surface points at */
static Window native_x11_window(const video_window_native_t *window)
{
   return window->state
      ? ((const x11_video_window_t*)window->state)->handle : None;
}

static int native_x11_ignore_error(Display *dpy, XErrorEvent *event)
{
   return 0;
}

static unsigned native_x11_monitors(Display *dpy, native_rect_t *out,
      unsigned capacity)
{
   unsigned n = 0;
#ifdef HAVE_XINERAMA
   int i, count             = 0;
   XineramaScreenInfo *info = XineramaQueryScreens(dpy, &count);
   for (i = 0; info && i < count && n < capacity; i++, n++)
   {
      out[n].x = info[i].x_org;
      out[n].y = info[i].y_org;
      out[n].w = info[i].width;
      out[n].h = info[i].height;
   }
   if (info)
      XFree(info);
#endif
   if (!n)
   {
      out[0].x = 0;
      out[0].y = 0;
      out[0].w = DisplayWidth(dpy, DefaultScreen(dpy));
      out[0].h = DisplayHeight(dpy, DefaultScreen(dpy));
      n        = 1;
   }
   return n;
}

static native_rect_t native_x11_rect(Display *dpy, Window win)
{
   native_rect_t rect;
   XWindowAttributes attrs;
   Window child;
   memset(&rect, 0, sizeof(rect));
   CHECK(XGetWindowAttributes(dpy, win, &attrs));
   XTranslateCoordinates(dpy, win, attrs.root, 0, 0, &rect.x, &rect.y,
         &child);
   rect.w = attrs.width;
   rect.h = attrs.height;
   return rect;
}

/* Pumps until the window is at 'want' in root coordinates, anywhere
 * with a zero 'want', and its record and last geometry event say so
 * too. */
static void native_x11_wait_rect(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, native_rect_t want)
{
   native_rect_t rect;
   bool anywhere         = !want.w;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      rect = native_x11_rect(g_x11_dpy,
            native_x11_window(window));
      if (anywhere)
         want = rect;
      if (     rect.x == want.x && rect.y == want.y
            && rect.w == want.w && rect.h == want.h
            && window->dims == VIDEO_SCALE_PACK(want.w, want.h)
            && native_window_pos == VIDEO_POS_PACK(want.x, want.y))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: at %d,%d %ux%u (event %d,%d), wanted "
         "%d,%d %ux%u\n", rect.x, rect.y, rect.w, rect.h,
         VIDEO_POS_X(native_window_pos), VIDEO_POS_Y(native_window_pos),
         want.x, want.y, want.w, want.h);
   CHECK(!"window bounds");
}

static bool native_x11_inside(native_rect_t monitor, native_rect_t rect)
{
   return    rect.x >= monitor.x && rect.y >= monitor.y
          && rect.x + (int)rect.w <= monitor.x + (int)monitor.w
          && rect.y + (int)rect.h <= monitor.y + (int)monitor.h;
}

static native_rect_t native_x11_centred(native_rect_t monitor,
      unsigned w, unsigned h)
{
   native_rect_t rect;
   rect.w = w;
   rect.h = h;
   rect.x = monitor.x + (int)(monitor.w - w) / 2;
   rect.y = monitor.y + (int)(monitor.h - h) / 2;
   return rect;
}

/* Pumps until the window is all on 'monitor' and its record agrees */
static void native_x11_wait_inside(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, native_rect_t monitor)
{
   native_rect_t rect;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      rect = native_x11_rect(g_x11_dpy,
            native_x11_window(window));
      if (     native_x11_inside(monitor, rect)
            && window->dims == VIDEO_SCALE_PACK(rect.w, rect.h)
            && native_window_pos == VIDEO_POS_PACK(rect.x, rect.y))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: at %d,%d %ux%u, wanted on %d,%d %ux%u\n",
         rect.x, rect.y, rect.w, rect.h,
         monitor.x, monitor.y, monitor.w, monitor.h);
   CHECK(!"window monitor");
}

static bool native_x11_primary_intact(Display *dpy, Window primary)
{
   XWindowAttributes attrs;
   return    g_x11_win == primary
          && XGetWindowAttributes(dpy, primary, &attrs)
          && attrs.map_state == IsViewable;
}

static bool native_x11_fullscreen_state(Display *dpy, Window win)
{
   Atom type;
   int format;
   unsigned long i;
   unsigned long n     = 0;
   unsigned long after = 0;
   unsigned char *data = NULL;
   bool found          = false;
   Atom fullscreen     = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
   if (     XGetWindowProperty(dpy, win,
               XInternAtom(dpy, "_NET_WM_STATE", False), 0, 64, False,
               XA_ATOM, &type, &format, &n, &after, &data) == Success
         && data)
      for (i = 0; i < n; i++)
         if (((Atom*)data)[i] == fullscreen)
            found = true;
   if (data)
      XFree(data);
   return found;
}

static void native_x11_wait_fullscreen(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      const video_window_native_t *window, bool want)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (native_x11_fullscreen_state(g_x11_dpy,
            native_x11_window(window)) != want)
   {
      if (cpu_features_get_time_usec() > deadline)
      {
         CHECK(!"fullscreen state");
         return;
      }
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
}

/* Windows named for 'screen', the root's children and theirs (a window
 * manager's frames) included */
static unsigned native_x11_titled(Display *dpy, unsigned screen)
{
   char want[64];
   Window root, parent;
   unsigned i, j;
   unsigned n     = 0;
   unsigned found = 0;
   Window *top    = NULL;
   int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(
         native_x11_ignore_error);
   snprintf(want, sizeof(want), "RetroArch - Screen %u", screen + 1);
   XSync(dpy, False);
   if (XQueryTree(dpy, DefaultRootWindow(dpy), &root, &parent, &top, &n))
   {
      for (i = 0; i < n; i++)
      {
         Window *sub = NULL;
         unsigned m  = 0;
         char *name  = NULL;
         if (XFetchName(dpy, top[i], &name) && name)
         {
            found += strcmp(name, want) == 0;
            XFree(name);
         }
         if (!XQueryTree(dpy, top[i], &root, &parent, &sub, &m))
            continue;
         for (j = 0; j < m; j++)
         {
            name = NULL;
            if (XFetchName(dpy, sub[j], &name) && name)
            {
               found += strcmp(name, want) == 0;
               XFree(name);
            }
         }
         if (sub)
            XFree(sub);
      }
      if (top)
         XFree(top);
   }
   XSync(dpy, False);
   XSetErrorHandler(old);
   return found;
}

/* 'win''s 'property' of 'type', "" for none */
static void native_x11_text(Display *dpy, Window win, const char *property,
      Atom type, char *out, size_t len)
{
   Atom actual;
   int format;
   unsigned long count, after;
   unsigned char *data = NULL;
   *out                = '\0';
   if (     XGetWindowProperty(dpy, win, XInternAtom(dpy, property, False),
               0, 64, False, type, &actual, &format, &count, &after,
               &data) == Success
         && data && actual == type && format == 8)
      strlcpy(out, (const char*)data, MIN(len, count + 1));
   if (data)
      XFree(data);
}

/* The title as each kind of window manager reads it: WM_NAME, and
 * EWMH's _NET_WM_NAME in UTF-8 */
static void native_x11_titles(Display *dpy, Window primary, Window win,
      unsigned screen)
{
   char name[128], net_name[128], main_name[128], main_net_name[128];
   Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
   native_x11_text(dpy, win, "WM_NAME", XA_STRING, name, sizeof(name));
   native_x11_text(dpy, win, "_NET_WM_NAME", utf8, net_name,
         sizeof(net_name));
   native_x11_text(dpy, primary, "WM_NAME", XA_STRING, main_name,
         sizeof(main_name));
   native_x11_text(dpy, primary, "_NET_WM_NAME", utf8, main_net_name,
         sizeof(main_net_name));
   native_window_titles(name, main_name, screen);
   native_window_titles(net_name, main_net_name, screen);
}

/* What a window manager sends when its close button is pressed */
static void native_x11_close(Display *display, Window window)
{
   XEvent event;
   memset(&event, 0, sizeof(event));
   event.xclient.type         = ClientMessage;
   event.xclient.window       = window;
   event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
   event.xclient.format       = 32;
   event.xclient.data.l[0]    = XInternAtom(display, "WM_DELETE_WINDOW", False);
   XSendEvent(display, window, False, NoEventMask, &event);
   XFlush(display);
}

/* Asks the window manager itself to close the window */
static void native_x11_wm_close(Display *display, Window window)
{
   XEvent event;
   memset(&event, 0, sizeof(event));
   event.xclient.type         = ClientMessage;
   event.xclient.window       = window;
   event.xclient.message_type = XInternAtom(display, "_NET_CLOSE_WINDOW",
         False);
   event.xclient.format       = 32;
   event.xclient.data.l[0]    = CurrentTime;
   event.xclient.data.l[1]    = 2;
   XSendEvent(display, DefaultRootWindow(display), False,
         SubstructureRedirectMask | SubstructureNotifyMask, &event);
   XFlush(display);
}

/* Asks for the keyboard focus on the window: through the window
 * manager when there is one, as a pager does */
static void native_x11_focus(Display *display, Window window, bool managed)
{
   XEvent event;
   if (!managed)
   {
      XSetInputFocus(display, window, RevertToParent, CurrentTime);
      XFlush(display);
      return;
   }
   memset(&event, 0, sizeof(event));
   event.xclient.type         = ClientMessage;
   event.xclient.window       = window;
   event.xclient.message_type = XInternAtom(display, "_NET_ACTIVE_WINDOW",
         False);
   event.xclient.format       = 32;
   event.xclient.data.l[0]    = 2;
   event.xclient.data.l[1]    = CurrentTime;
   XSendEvent(display, DefaultRootWindow(display), False,
         SubstructureRedirectMask | SubstructureNotifyMask, &event);
   XFlush(display);
}

/* Focus on the main window is RetroArch's; focus on a window of
 * another client, made here, is not */
static void native_x11_has_focus(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, Display *display,
      Window primary, bool managed)
{
   XWindowAttributes attrs;
   retro_time_t deadline;
   Window foreign;

   native_x11_focus(display, primary, managed);
   CHECK(native_window_has_focus(ctx, data, request, true));
   foreign  = XCreateSimpleWindow(display, DefaultRootWindow(display),
         0, 0, 64, 64, 0, 0, 0);
   XMapWindow(display, foreign);
   XSync(display, False);
   deadline = cpu_features_get_time_usec() + 3000000;
   while (     XGetWindowAttributes(display, foreign, &attrs)
         && attrs.map_state != IsViewable
         && cpu_features_get_time_usec() < deadline)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   native_x11_focus(display, foreign, managed);
   CHECK(native_window_has_focus(ctx, data, request, false));
   XDestroyWindow(display, foreign);
   XSync(display, False);
}

/* Pumps until the window has the keyboard focus */
static void native_x11_focused(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, Display *display,
      Window win, bool managed)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   native_x11_focus(display, win, managed);
   while (!window->focused && cpu_features_get_time_usec() < deadline)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   native_window_settle(ctx, data, request, 100000);
   CHECK(window->focused);
}

/* A down (1) and an up (2) of A, as the core's keyboard callback has them */
static unsigned native_x11_key_seen;

static void native_x11_key_event(bool down, unsigned code,
      uint32_t character, uint16_t mod)
{
   (void)character;
   (void)mod;
   if (code == RETROK_a)
      native_x11_key_seen |= down ? 1 : 2;
}

static void native_x11_send_key(Display *display, Window win,
      unsigned keycode, bool down)
{
   XEvent event;
   memset(&event, 0, sizeof(event));
   event.xkey.type        = down ? KeyPress : KeyRelease;
   event.xkey.display     = display;
   event.xkey.window      = win;
   event.xkey.root        = DefaultRootWindow(display);
   event.xkey.time        = CurrentTime;
   event.xkey.keycode     = keycode;
   event.xkey.same_screen = True;
   XSendEvent(display, win, False,
         down ? KeyPressMask : KeyReleaseMask, &event);
   XFlush(display);
}

/* Pumps and polls until A reads 'down' in the X input driver and the
 * core has had its event; sent again each time, as the driver selects
 * the window at its poll */
static void native_x11_key_wait(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, void *in, Display *display,
      Window win, bool down)
{
   bool pressed          = !down;
   unsigned want         = down ? 1 : 2;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   do
   {
      native_x11_send_key(display, win, rarch_keysym_lut[RETROK_a], down);
      retro_sleep(20);
      native_window_drain(ctx, data, request);
      input_x.poll(in);
      pressed = input_x.input_state(in, NULL, NULL, NULL, 0,
            RETRO_DEVICE_KEYBOARD, 0, RETROK_a) != 0;
   } while (   (pressed != down || !(native_x11_key_seen & want))
            && cpu_features_get_time_usec() < deadline);
   CHECK(pressed == down);
   CHECK(native_x11_key_seen & want);
}

/* Keys typed in a screen's own window reach the X input driver's own
 * connection and the main window's key path */
static void native_x11_keys(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, Display *display, Window win)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   retro_keyboard_event_t saved = runloop_st->key_event;
   void *in                     = input_x.init(NULL);
   CHECK(in != NULL);
   if (!in)
      return;
   runloop_st->key_event = native_x11_key_event;
   native_x11_key_seen   = 0;
   native_x11_key_wait(ctx, data, request, in, display, win, true);
   native_x11_key_wait(ctx, data, request, in, display, win, false);
   runloop_st->key_event = saved;
   input_x.free(in);
}

/* An error in 'window' handed to 'handler' as Xlib would hand it */
static void native_x11_fake_error(int (*handler)(Display*, XErrorEvent*),
      Display *display, Window window, int code)
{
   XErrorEvent event;
   memset(&event, 0, sizeof(event));
   event.display      = display;
   event.resourceid   = window;
   event.serial       = XNextRequest(display) + 100000;
   event.error_code   = code;
   event.request_code = 12; /* X_ConfigureWindow */
   CHECK(handler(display, &event) == 0);
}

/* Errors the window handler handed on */
static unsigned native_x11_chained;

static int native_x11_count_error(Display *dpy, XErrorEvent *event)
{
   native_x11_chained++;
   return 0;
}

/* On another connection, as the input driver's, only a BadWindow for a
 * screen's window is dropped; the rest is handed on */
static void native_x11_errors_elsewhere(Display *display, Window window)
{
   int (*handler)(Display*, XErrorEvent*);
   Display *other = XOpenDisplay(DisplayString(display));
   CHECK(other != NULL);
   if (!other)
      return;
   handler = XSetErrorHandler(native_x11_ignore_error);
   XSetErrorHandler(handler);
   CHECK(handler != native_x11_count_error);
   native_x11_chained = 0;
   native_x11_fake_error(handler, other, window, BadWindow);
   CHECK(native_x11_chained == 0);
   native_x11_fake_error(handler, other, window, BadMatch);
   CHECK(native_x11_chained == 1);
   native_x11_fake_error(handler, other, DefaultRootWindow(display),
         BadWindow);
   CHECK(native_x11_chained == 2);
   XCloseDisplay(other);
}

/* A preferred monitor, with saved bounds that are not on it: moved onto
 * it where the bounds reach it, centred where they do not */
static void native_x11_preferred(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      const native_rect_t *monitors, unsigned count)
{
   request->settings.monitor = count;
   request->settings.pos     = VIDEO_POS_PACK(-200, -200);
   request->settings.pos_set = true;
   ctx->windows->update(data, window, request);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
         VIDEO_SCALE_PACK(320, 240));
   if (monitors[count - 1].x >= 120 || monitors[count - 1].y >= 40)
      native_x11_wait_rect(ctx, data, request, window,
            native_x11_centred(monitors[count - 1], 320, 240));
   else
      native_x11_wait_inside(ctx, data, request, window,
            monitors[count - 1]);
}

#ifdef NATIVE_EGL
static void *native_x11_drawable(const video_window_native_t *window)
{
   return ((x11_video_window_t*)window->state)->drawable;
}
#endif

#ifdef NATIVE_GLX
/* A GLX drawable that cannot be made current fails its window, which
 * is then replaced: here an XID the server no longer has, bound while
 * the window's own drawable is current, after which the main window's
 * is current again. */
static void native_glx_failure(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   bool bound;
   Window gone;
   int (*old)(Display*, XErrorEvent*);
   Display *dpy              = g_x11_dpy;
   x11_video_window_t *state = (x11_video_window_t*)window->state;
   void *drawable            = state->drawable;
   GLXDrawable primary       = glXGetCurrentDrawable();
   GLXContext context        = glXGetCurrentContext();
   CHECK(window->presentable);
   CHECK(ctx->windows->bind_gl(data, window));
   gone = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0,
         0, 0);
   XDestroyWindow(dpy, gone);
   XSync(dpy, False);
   old             = XSetErrorHandler(native_x11_ignore_error);
   state->drawable = (void*)(uintptr_t)gone;
   bound           = ctx->windows->bind_gl(data, window);
   state->drawable = drawable;
   XSync(dpy, False);
   XSetErrorHandler(old);
   if (bound)
   {
      fprintf(stderr, "native x11: GLX made a drawable that is gone current,"
            " bind failure not forced\n");
      CHECK(ctx->windows->bind_gl(data, NULL));
      return;
   }
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(!native_window_closed_user);
   CHECK(!window->presentable);
   CHECK(!ctx->windows->bind_gl(data, window));
   CHECK(glXGetCurrentDrawable() == primary);
   CHECK(glXGetCurrentContext() == context);
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}
#endif

/* Pumps until a GEOMETRY with bounds to save arrives; false at the
 * deadline */
static bool native_x11_wait_save(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request)
{
   unsigned saves        = native_window_saves;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (native_window_saves == saves)
   {
      if (cpu_features_get_time_usec() > deadline)
         return false;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   return true;
}

/* Screen 1's window made anew from 'request', of a new generation */
static Window native_x11_remake(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   return native_x11_window(window);
}

/* Maximised or not: through the window manager when there is one, else
 * the state a manager would set */
static void native_x11_maximise(Display *dpy, Window win, bool on,
      bool managed)
{
   Atom state = XInternAtom(dpy, "_NET_WM_STATE", False);
   Atom max[2];
   max[0]     = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
   max[1]     = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
   if (managed)
   {
      XEvent event;
      memset(&event, 0, sizeof(event));
      event.xclient.type         = ClientMessage;
      event.xclient.window       = win;
      event.xclient.message_type = state;
      event.xclient.format       = 32;
      /* _NET_WM_STATE_ADD or _REMOVE */
      event.xclient.data.l[0]    = on ? 1 : 0;
      event.xclient.data.l[1]    = (long)max[0];
      event.xclient.data.l[2]    = (long)max[1];
      event.xclient.data.l[3]    = 1;
      XSendEvent(dpy, DefaultRootWindow(dpy), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &event);
   }
   else if (on)
      XChangeProperty(dpy, win, state, XA_ATOM, 32, PropModeReplace,
            (unsigned char*)max, 2);
   else
      XDeleteProperty(dpy, win, state);
   XFlush(dpy);
}

/* A manager that configures before it sets _NET_WM_STATE, from 'fix'
 * on a bare server: the state that comes with the configure, or with
 * the answer for a reparented window's root position, keeps the
 * maximised bounds unsaved. Leaves it at x+300,y+200 420x310 again. */
static void native_x11_max_first(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *r, video_window_native_t *window,
      Display *fix, native_rect_t monitor)
{
   video_window_event_t events[4];
   unsigned i, count;
   x11_video_window_t *state;
   Window frame;
   unsigned saves = native_window_saves;
   unsigned dims  = VIDEO_SCALE_PACK(640, 480);
   Window win     = native_x11_window(window);
   int x          = monitor.x;
   int y          = monitor.y;
   bool quit      = false;
   bool resize    = false;

   /* The root's child: both in one pump */
   XMoveResizeWindow(fix, win, x, y, monitor.w, monitor.h);
   native_x11_maximise(fix, win, true, false);
   native_window_settle(ctx, data, r, 200000);
   CHECK(native_window_saves == saves);
   /* Back to the bounds last saved, so whether the state's answer has
    * come by then makes no difference */
   XMoveResizeWindow(fix, win, x + 260, y + 170, 420, 310);
   native_x11_maximise(fix, win, false, false);
   native_window_settle(ctx, data, r, 200000);
   CHECK(native_window_saves == saves);

   /* Reparented: the state lands with the answer, after the pump that
    * asked; one poll per pump, as a renderer's frame */
   frame = XCreateSimpleWindow(fix, DefaultRootWindow(fix), x + 40, y + 40,
         monitor.w - 80, monitor.h - 80, 0, 0, 0);
   XMapWindow(fix, frame);
   XReparentWindow(fix, win, frame, 10, 10);
   XSync(fix, False);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   state = (x11_video_window_t*)window->state;
   CHECK(state && state->reparented);
   saves = native_window_saves;
   XMoveResizeWindow(fix, win, 0, 0, 700, 500);
   XSync(fix, False);
   ctx->check_window(data, &quit, &resize, &dims);
   retro_sleep(20);
   native_x11_maximise(fix, win, true, false);
   XSync(fix, False);
   retro_sleep(20);
   count = ctx->windows->poll(data, events, 4);
   for (i = 0; i < count; i++)
      if (     events[i].kind == VIDEO_WINDOW_EVENT_GEOMETRY
            && events[i].save_dims)
         native_window_saves++;
   native_window_settle(ctx, data, r, 200000);
   CHECK(native_window_saves == saves);

   XReparentWindow(fix, win, DefaultRootWindow(fix), x + 300, y + 200);
   XResizeWindow(fix, win, 420, 310);
   native_x11_maximise(fix, win, false, false);
   XDestroyWindow(fix, frame);
   XSync(fix, False);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
}

/* Takes events and answers without poll() until the window has bounds
 * to judge and a _NET_WM_STATE read out, or with 'again' until another
 * is to be asked once that is answered */
static bool native_x11_pump_reads(const gfx_ctx_driver_t *ctx,
      void *data, const x11_video_window_t *state, bool again)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (again ? !state->wm_state_again
         : !(state->judge && state->wm_state))
   {
      bool quit     = false;
      bool resize   = false;
      unsigned dims = VIDEO_SCALE_PACK(640, 480);
      if (cpu_features_get_time_usec() > deadline)
         return false;
      ctx->check_window(data, &quit, &resize, &dims);
      retro_sleep(2);
   }
   return true;
}

/* One poll() while 'fix' grabs the server, so the read is still out;
 * false if it judged the bounds */
static bool native_x11_held(const gfx_ctx_driver_t *ctx, void *data,
      const x11_video_window_t *state)
{
   video_window_event_t events[4];
   unsigned i;
   unsigned count = ctx->windows->poll(data, events, 4);
   for (i = 0; i < count; i++)
      if (     events[i].kind == VIDEO_WINDOW_EVENT_GEOMETRY
            && events[i].save_dims)
         native_window_saves++;
   return state->judge;
}

/* Bounds that come while a _NET_WM_STATE read is out wait for it, and
 * only for it: a move is saved, and a move while maximised is not, even
 * when it is unmaximised before poll() has the answer, with a poll()
 * before the answer came or none. Leaves the window at x+320,y+200
 * 420x310. */
static void native_x11_state_out(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *r, video_window_native_t *window,
      Display *fix, native_rect_t monitor)
{
   int i;
   unsigned saves            = native_window_saves;
   x11_video_window_t *state = (x11_video_window_t*)window->state;
   Window win                = native_x11_window(window);
   if (!state)
   {
      CHECK(!"window state");
      return;
   }

   XGrabServer(fix);
   XChangeProperty(fix, win, XInternAtom(fix, "_NET_WM_STATE", False),
         XA_ATOM, 32, PropModeReplace, (unsigned char*)&win, 0);
   XMoveWindow(fix, win, monitor.x + 280, monitor.y + 190);
   XSync(fix, False);
   CHECK(native_x11_pump_reads(ctx, data, state, false));
   CHECK(native_x11_held(ctx, data, state));
   XUngrabServer(fix);
   XSync(fix, False);
   CHECK(native_window_saves == saves);
   CHECK(native_x11_wait_save(ctx, data, r));
   CHECK(native_window_saved.save_pos
         == VIDEO_POS_PACK(monitor.x + 280, monitor.y + 190));

   /* The answer, maximised, waits unread while the unmaximise comes */
   for (i = 0; i < 2; i++)
   {
      saves = native_window_saves;
      XGrabServer(fix);
      native_x11_maximise(fix, win, true, false);
      XMoveWindow(fix, win, monitor.x + 300 + i * 20, monitor.y + 200);
      XSync(fix, False);
      CHECK(native_x11_pump_reads(ctx, data, state, false));
      if (!i)
         CHECK(native_x11_held(ctx, data, state));
      XUngrabServer(fix);
      XSync(fix, False);
      XSync(g_x11_dpy, False);
      native_x11_maximise(fix, win, false, false);
      XSync(fix, False);
      CHECK(native_x11_pump_reads(ctx, data, state, true));
      native_window_settle(ctx, data, r, 300000);
      CHECK(native_window_saves == saves);
   }
}

/* GEOMETRY saves where the user left a window: a move or a resize, but
 * nothing while it is maximised or fullscreen, nor where the window
 * manager places it. The user's moves come from a client of their own,
 * as a window manager's do. Leaves a window of 'request' up. */
static void native_x11_saves(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      native_rect_t monitor, bool managed)
{
   static bool done;
   video_window_request_t r;
   native_rect_t want;
   x11_video_window_t *state;
   Window win;
   unsigned saves;
   unsigned saved_pos  = VIDEO_POS_PACK(monitor.x + 260, monitor.y + 170);
   unsigned saved_dims = VIDEO_SCALE_PACK(400, 300);
   int x               = monitor.x;
   int y               = monitor.y;
   Display *fix;

   if (done)
      return;
   done = true;
   if (!(fix = XOpenDisplay(DisplayString(g_x11_dpy))))
   {
      CHECK(!"fixture display");
      return;
   }
   r                     = *request;
   r.settings.monitor    = 0;
   r.settings.fullscreen = false;
   r.settings.pos        = VIDEO_POS_PACK(x + 200, y + 150);
   r.settings.dims       = VIDEO_SCALE_PACK(400, 300);
   r.settings.pos_set    = true;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   saves = native_window_saves;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);

   /* A drag */
   XMoveResizeWindow(fix, win, x + 260, y + 170, 400, 300);
   XFlush(fix);
   CHECK(native_x11_wait_save(ctx, data, &r));
   CHECK(native_window_saved.save_moved);
   CHECK(native_window_saved.save_pos == VIDEO_POS_PACK(x + 260, y + 170));
   CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(400, 300));
   if (native_window_saved.save_moved)
   {
      saved_pos  = native_window_saved.save_pos;
      saved_dims = native_window_saved.save_dims;
   }

   /* A resize in place: the size alone, once */
   XResizeWindow(fix, win, 420, 310);
   XFlush(fix);
   CHECK(native_x11_wait_save(ctx, data, &r));
   CHECK(!native_window_saved.save_moved);
   CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(420, 310));
   saves = native_window_saves;
   native_window_settle(ctx, data, &r, 200000);
   CHECK(native_window_saves == saves);

   /* Maximised, moved there, and back: nothing */
   native_x11_maximise(fix, win, true, managed);
   if (managed)
   {
      native_window_settle(ctx, data, &r, 500000);
      CHECK(window->dims != VIDEO_SCALE_PACK(420, 310));
   }
   else
   {
      XMoveWindow(fix, win, x + 300, y + 200);
      XFlush(fix);
      want.x = x + 300;
      want.y = y + 200;
      want.w = 420;
      want.h = 310;
      native_x11_wait_rect(ctx, data, &r, window, want);
   }
   native_x11_maximise(fix, win, false, managed);
   if (managed)
      native_window_wait(ctx, data, &r, window, NATIVE_EVENT(GEOMETRY),
            VIDEO_SCALE_PACK(420, 310));
   native_window_settle(ctx, data, &r, 200000);
   CHECK(native_window_saves == saves);
   if (!managed)
   {
      native_x11_max_first(ctx, data, &r, window, fix, monitor);
      native_x11_state_out(ctx, data, &r, window, fix, monitor);
      saves = native_window_saves;
   }

   /* Fullscreen, moved: nothing */
   r.settings.fullscreen = true;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   XMoveResizeWindow(fix, win, x + 100, y + 100, 300, 200);
   XFlush(fix);
   native_window_settle(ctx, data, &r, 300000);
   CHECK(native_window_saves == saves);
   r.settings.fullscreen = false;

   /* Moved at once after it was made, as a window manager places it */
   win = native_x11_remake(ctx, data, &r, window);
   XMoveWindow(fix, win, x + 240, y + 190);
   XFlush(fix);
   native_window_settle(ctx, data, &r, 600000);
   CHECK(native_window_saves == saves);

   /* No position set: where the window manager puts it is no move.
    * create() sets USPosition, so a manager keeps RetroArch's spot. */
   r.settings.pos_set = false;
   native_x11_remake(ctx, data, &r, window);
   native_window_settle(ctx, data, &r, 600000);
   CHECK(native_window_saves == saves);
   state = (x11_video_window_t*)window->state;
   CHECK(state && !state->common.request.settings.pos_set);
   CHECK(state && state->common.request.settings.dims == r.settings.dims);

   /* Saved bounds make the window they were saved from, and it reports
    * them */
   r.settings.pos     = saved_pos;
   r.settings.dims    = saved_dims;
   r.settings.pos_set = true;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   want = native_x11_rect(g_x11_dpy, win);
   if (     want.x != VIDEO_POS_X(saved_pos) || want.y != VIDEO_POS_Y(saved_pos)
         || want.w != VIDEO_SCALE_W(saved_dims)
         || want.h != VIDEO_SCALE_H(saved_dims))
      fprintf(stderr, "native x11: remade at %d,%d %ux%u from %d,%d %ux%u\n",
            want.x, want.y, want.w, want.h,
            VIDEO_POS_X(saved_pos), VIDEO_POS_Y(saved_pos),
            VIDEO_SCALE_W(saved_dims), VIDEO_SCALE_H(saved_dims));
   CHECK(VIDEO_POS_PACK(want.x, want.y) == saved_pos);
   CHECK(VIDEO_SCALE_PACK(want.w, want.h) == saved_dims);
   state = (x11_video_window_t*)window->state;
   CHECK(state && state->common.pos == saved_pos);
   CHECK(window->dims == saved_dims);
   CHECK(native_window_saves == saves);

   XCloseDisplay(fix);
   native_x11_remake(ctx, data, request, window);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}

/* Fullscreen on and off through update(): it fills 'monitor', comes back
 * at 'windowed', and none of it is a move of the user's. Each update()
 * sends the move and the state, so never a round trip. */
static void native_x11_toggle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *r, video_window_native_t *window,
      native_rect_t monitor, native_rect_t windowed, bool managed)
{
   unsigned long requests;
   unsigned saves        = native_window_saves;

   r->settings.fullscreen = true;
   requests               = XNextRequest(g_x11_dpy);
   ctx->windows->update(data, window, r);
   CHECK(XNextRequest(g_x11_dpy) - requests <= 2);
   native_x11_wait_rect(ctx, data, r, window, monitor);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, r, window, true);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);

   r->settings.fullscreen = false;
   requests               = XNextRequest(g_x11_dpy);
   ctx->windows->update(data, window, r);
   CHECK(XNextRequest(g_x11_dpy) - requests <= 2);
   native_x11_wait_rect(ctx, data, r, window, windowed);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, r, window, false);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   native_x11_wait_rect(ctx, data, r, window, windowed);
}

/* Takes the window's events and answers, leaving what poll() has for
 * GEOMETRY unread, until a drag's bounds wait for poll() to judge them
 * or, with 'judged', wait to go out */
static bool native_x11_wait_held(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_native_t *window, bool judged)
{
   video_window_event_t none;
   x11_video_window_t *state = (x11_video_window_t*)window->state;
   retro_time_t deadline     = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      bool quit   = false;
      bool resize = false;
      unsigned dims = VIDEO_SCALE_PACK(640, 480);
      ctx->check_window(data, &quit, &resize, &dims);
      if (judged ? state->common.save_dims != 0 : state->judge)
         return true;
      ctx->windows->poll(data, &none, 0);
      if (judged ? state->common.save_dims != 0 : state->judge)
         return true;
      if (cpu_features_get_time_usec() > deadline)
         return false;
      retro_sleep(2);
   }
}

/* An edit as the renderers hand it on. What update() puts a window at
 * is not the user's, and the same request again moves nothing. */
static void native_x11_updates(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      native_rect_t monitor, unsigned ordinal, bool managed)
{
   static bool done;
   video_window_request_t r;
   native_rect_t want;
   XEvent event;
   x11_video_window_t *state;
   Display *fix;
   Window win;
   unsigned i, saves;
   unsigned long requests;
   int x = monitor.x;
   int y = monitor.y;

   if (done)
      return;
   done = true;
   if (!(fix = XOpenDisplay(DisplayString(g_x11_dpy))))
   {
      CHECK(!"fixture display");
      return;
   }
   r                     = *request;
   r.settings.monitor    = 0;
   r.settings.fullscreen = false;
   r.settings.pos        = VIDEO_POS_PACK(x + 200, y + 150);
   r.settings.dims       = VIDEO_SCALE_PACK(400, 300);
   r.settings.pos_set    = true;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);

   /* Dragged, so the next requests carry its saved bounds */
   XMoveResizeWindow(fix, win, x + 260, y + 170, 400, 300);
   XFlush(fix);
   CHECK(native_x11_wait_save(ctx, data, &r));
   r.settings.pos  = native_window_saved.save_pos;
   r.settings.dims = native_window_saved.save_dims;
   want.x          = x + 260;
   want.y          = y + 170;
   want.w          = 400;
   want.h          = 300;
   native_x11_wait_rect(ctx, data, &r, window, want);
   native_x11_toggle(ctx, data, &r, window, monitor, want, managed);
   saves = native_window_saves;

   /* The X and Y rows, then the Size row */
   r.settings.pos = VIDEO_POS_PACK(x + 300, y + 200);
   want.x         = x + 300;
   want.y         = y + 200;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, want);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   r.settings.dims = VIDEO_SCALE_PACK(420, 310);
   want.w          = 420;
   want.h          = 310;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, want);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);

   /* The same request, twice: nothing goes out, nothing moves */
   XSelectInput(fix, win, StructureNotifyMask);
   XSync(fix, False);
   while (XPending(fix))
      XNextEvent(fix, &event);
   requests = XNextRequest(g_x11_dpy);
   ctx->windows->update(data, window, &r);
   ctx->windows->update(data, window, &r);
   CHECK(XNextRequest(g_x11_dpy) == requests);
   native_window_settle(ctx, data, &r, 200000);
   XSync(fix, False);
   CHECK(!XCheckTypedWindowEvent(fix, win, ConfigureNotify, &event));
   XSelectInput(fix, win, 0);
   CHECK(native_window_saves == saves);

   /* A drag on its way when an edit comes, its bounds still to be judged
    * and then already judged: the edit stands */
   state = (x11_video_window_t*)window->state;
   for (i = 0; i < 2; i++)
   {
      XMoveResizeWindow(fix, win, x + 340 + (int)i * 10, y + 220,
            want.w, want.h);
      XFlush(fix);
      CHECK(native_x11_wait_held(ctx, data, window, i == 1));
      r.settings.pos = VIDEO_POS_PACK(x + 180 + (int)i * 10, y + 120);
      want.x         = x + 180 + (int)i * 10;
      want.y         = y + 120;
      ctx->windows->update(data, window, &r);
      native_x11_wait_rect(ctx, data, &r, window, want);
      native_window_settle(ctx, data, &r,
            VIDEO_WINDOW_SETTLE_USEC + 100000);
      CHECK(native_window_saves == saves);
      CHECK(state && state->common.request.settings.pos == r.settings.pos);
      CHECK(native_x11_rect(fix, win).x == want.x);
   }

   /* Settled again, the user's moves are saved */
   XMoveResizeWindow(fix, win, x + 360, y + 230, want.w, want.h);
   XFlush(fix);
   CHECK(native_x11_wait_save(ctx, data, &r));
   CHECK(native_window_saved.save_pos == VIDEO_POS_PACK(x + 360, y + 230));
   saves = native_window_saves;

   /* Never dragged, automatic and then with a size alone: where it was,
    * not centred, and it gets no bounds */
   for (i = 0; i < 2; i++)
   {
      r.settings.pos     = 0;
      r.settings.dims    = i ? VIDEO_SCALE_PACK(400, 300) : 0;
      r.settings.pos_set = false;
      win = native_x11_remake(ctx, data, &r, window);
      XMoveWindow(fix, win, x + 240, y + 190);
      XFlush(fix);
      native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
      want   = native_x11_rect(fix, win);
      want.x = x + 240;
      want.y = y + 190;
      native_x11_wait_rect(ctx, data, &r, window, want);
      native_window_settle(ctx, data, &r,
            VIDEO_WINDOW_SETTLE_USEC + 100000);
      CHECK(native_window_saves == saves);
      native_x11_toggle(ctx, data, &r, window, monitor, want, managed);
      state = (x11_video_window_t*)window->state;
      CHECK(state && !state->common.request.settings.pos_set);
   }

   /* An Auto window's first X or Y row: to no monitor, with a position */
   r.settings.monitor = ordinal;
   r.settings.pos     = 0;
   r.settings.dims    = VIDEO_SCALE_PACK(400, 300);
   r.settings.pos_set = false;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   /* It came up where it was put, which no GEOMETRY says */
   state = (x11_video_window_t*)window->state;
   if (state)
      native_window_pos = state->common.pos;
   native_x11_wait_rect(ctx, data, &r, window,
         native_x11_centred(monitor, 400, 300));
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   r.settings.monitor = 0;
   r.settings.pos     = VIDEO_POS_PACK(x + 200, y + 150);
   r.settings.pos_set = true;
   want.x             = x + 200;
   want.y             = y + 150;
   want.w             = 400;
   want.h             = 300;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, want);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   CHECK(native_x11_window(window) == win);
   state = (x11_video_window_t*)window->state;
   CHECK(state && !state->common.request.settings.monitor);

   /* Made fullscreen it has no rect to go back to: its request's */
   r.settings.pos        = 0;
   r.settings.pos_set    = false;
   r.settings.fullscreen = true;
   native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   native_window_settle(ctx, data, &r, 200000);
   r.settings.fullscreen = false;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window,
         native_x11_centred(monitor, 400, 300));
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);

   /* A size and no position: a size asked for while fullscreen, then
    * with Fullscreen on, comes back where the window stood */
   r.settings.monitor = 0;
   win = native_x11_remake(ctx, data, &r, window);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   state = (x11_video_window_t*)window->state;
   if (state)
      native_window_pos = state->common.pos;
   want = native_x11_centred(monitor, 400, 300);
   native_x11_wait_rect(ctx, data, &r, window, want);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   r.settings.fullscreen = true;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, monitor);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, &r, window, true);
   r.settings.dims = VIDEO_SCALE_PACK(500, 360);
   ctx->windows->update(data, window, &r);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_x11_wait_rect(ctx, data, &r, window, monitor);
   CHECK(native_window_saves == saves);
   r.settings.fullscreen = false;
   want.w                = 500;
   want.h                = 360;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, want);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, &r, window, false);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   native_x11_wait_rect(ctx, data, &r, window, want);

   r.settings.dims       = VIDEO_SCALE_PACK(520, 370);
   r.settings.fullscreen = true;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, monitor);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, &r, window, true);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   r.settings.fullscreen = false;
   want.w                = 520;
   want.h                = 370;
   ctx->windows->update(data, window, &r);
   native_x11_wait_rect(ctx, data, &r, window, want);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, &r, window, false);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_window_saves == saves);
   native_x11_wait_rect(ctx, data, &r, window, want);

   XCloseDisplay(fix);
   native_x11_remake(ctx, data, request, window);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}

#ifdef RARCH_HAVE_X11_MONITORS
/* What the worker should find: Xinerama's heads, asked here as a test */
static unsigned native_x11_heads(Display *dpy)
{
   int count                = 0;
   XineramaScreenInfo *info = XineramaQueryScreens(dpy, &count);
   if (!info)
      return 0;
   XFree(info);
   return (unsigned)count < VIDEO_WINDOW_DISPLAYS_MAX
      ? (unsigned)count : VIDEO_WINDOW_DISPLAYS_MAX;
}

#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
static void native_x11_rr_set(Display *dpy, const char *name, int x,
      int w, RROutput output)
{
   XRRMonitorInfo *mon = XRRAllocateMonitor(dpy, output ? 1 : 0);
   int h               = DisplayHeight(dpy, DefaultScreen(dpy));
   mon->name           = XInternAtom(dpy, name, False);
   mon->x              = x;
   mon->y              = 0;
   mon->width          = w;
   mon->height         = h;
   mon->mwidth         = w * 254 / 960;
   mon->mheight        = h * 286 / 1080;
   if (output)
      mon->outputs[0]  = output;
   XRRSetMonitor(dpy, DefaultRootWindow(dpy), mon);
   XRRFreeMonitors(mon);
}
#endif

#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
/* RA-A, first in Xinerama's order, at @a with the output, RA-B at @b.
 * Set again, they go first: a monitor named again cannot take an
 * output it has. XRRSetMonitor sends no RandR event, so the worker is
 * told. */
static void native_x11_two(Display *dpy, int a, int b, int w)
{
   RROutput output         = 0;
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy,
         DefaultRootWindow(dpy));
   if (native_x11_heads(dpy) >= 2)
   {
      XRRDeleteMonitor(dpy, DefaultRootWindow(dpy),
            XInternAtom(dpy, "RA-B", False));
      XRRDeleteMonitor(dpy, DefaultRootWindow(dpy),
            XInternAtom(dpy, "RA-A", False));
   }
   if (res)
   {
      if (res->noutput > 0)
         output = res->outputs[0];
      XRRFreeScreenResources(res);
   }
   native_x11_rr_set(dpy, "RA-A", a, w, output);
   native_x11_rr_set(dpy, "RA-B", b, w, 0);
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_ENUMERATE);
}
#endif

/* NATIVE_X11_MONITORS=2: the screen as two monitors, RA-A on the left
 * with the output, RA-B on the right. True when split here. */
static bool native_x11_split(Display *dpy)
{
   const char *want = getenv("NATIVE_X11_MONITORS");
   /* Split already by the last lifetime: RA-A holds the output */
   if (!want || strcmp(want, "2") != 0 || native_x11_heads(dpy) >= 2)
      return false;
#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
   {
      int half = DisplayWidth(dpy, DefaultScreen(dpy)) / 2;
      native_x11_two(dpy, 0, half, half);
   }
#endif
   fprintf(stderr, "native x11: split into %u Xinerama head(s)\n",
         native_x11_heads(dpy));
   return true;
}

/* The published list is Xinerama's heads in order, one of them the
 * main window's, or with @own >= 0 that one */
static bool native_x11_monitors_match(const native_rect_t *monitors,
      unsigned want, int own, const video_window_display_t *got,
      unsigned n)
{
   unsigned i;
   unsigned owned = 0;
   if (n != want)
      return false;
   for (i = 0; i < n; i++)
   {
      if (     got[i].ordinal < 1
            || got[i].dims != VIDEO_SCALE_PACK(monitors[i].w, monitors[i].h)
            || string_is_equal(got[i].name, "foreign"))
         return false;
      if (got[i].own)
         owned++;
   }
   return    (!n || owned == 1)
          && (own < 0 || ((unsigned)own < n && got[own].own));
}

/* Reads the list as the app thread does, pumping until it matches */
static unsigned native_x11_monitors_wait(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      const native_rect_t *monitors, unsigned want, int own,
      video_window_display_t *got)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   unsigned n;
   for (;;)
   {
      n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
      if (native_x11_monitors_match(monitors, want, own, got, n))
         return n;
      if (cpu_features_get_time_usec() > deadline)
         break;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   fprintf(stderr, "native x11: %u monitor(s) published for %u head(s),"
         " own wanted %d\n", n, want, own);
   CHECK(!"published monitors");
   return n;
}

/* Until the worker's serial moves past @serial */
static void native_x11_serial_wait(unsigned serial)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (     x11_monitors_serial() == serial
         && cpu_features_get_time_usec() < deadline)
      retro_sleep(2);
   CHECK(x11_monitors_serial() != serial);
}

/* Until the worker has located the main window */
static void native_x11_located_wait(void)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   int own;
   for (;;)
   {
      const x11_monitors_t *snap = x11_monitors_get();
      own                        = snap ? snap->own : -1;
      x11_monitors_put();
      if (own >= 0 || cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   CHECK(own >= 0);
}

/* Off, the screens' windows cost no thread and no X connection of
 * their own. Turned on, the context's own pump starts the worker, and
 * the screens wait in the main window for its first list: their windows
 * are made once, where it puts them. Needs the toggle off at connect. */
static void native_x11_worker_gate(const gfx_ctx_driver_t *ctx,
      void *data, video_window_request_t *request,
      video_window_native_t *window, const native_rect_t *monitors)
{
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   retro_time_t deadline;
   unsigned i, n;
   unsigned waited = 0;
   unsigned first  = 0;
   unsigned again;
   settings_t *settings = config_get_ptr();
   CHECK(!settings->bools.video_windows_enable);
   native_window_settle(ctx, data, request, 200000);
   n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   fprintf(stderr, "native x11: screen windows off: worker serial %u,"
         " state %d, %u monitor(s) published\n", x11_monitors_serial(),
         x11_monitors_state(), n);
   /* -1: the worker native_x11_unlisted() starved gave up */
   CHECK(x11_monitors_serial() == 0 && x11_monitors_state() <= 0);
   CHECK(n == 0);
   window_test_begin();
   deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      window_test_run();
      if (window_test_handed && (window_test_seen.plan.separate_mask & 2u))
         break;
      CHECK(window_test_placements() == 2);
      if (cpu_features_get_time_usec() > deadline)
         break;
      waited++;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   if (window_test_handed)
      first = window_test_seen.settings[1].monitor;
   again = first;
   fprintf(stderr, "native x11: first window after %u frame(s) in the"
         " main window, on monitor %u\n", waited, first);
   CHECK(x11_monitors_serial() != 0);
   CHECK(first != 0);
   /* Not made again once the list is in */
   for (i = 0; i < 20 && again == first; i++)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
      window_test_run();
      again = window_test_handed ? window_test_seen.settings[1].monitor : 0;
   }
   if (again != first)
      fprintf(stderr, "native x11: made again on monitor %u\n", again);
   CHECK(again == first);
   window_test_end();
   /* Where the renderer makes it; with one monitor it does not move */
   n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   for (i = 0; i < n && first; i++)
      if (got[i].ordinal == first)
      {
         native_rect_t rect;
         video_window_settings_t was = request->settings;
         request->settings.monitor   = first;
         ctx->windows->update(data, window, request);
         deadline = cpu_features_get_time_usec() + 3000000;
         do
         {
            native_window_drain(ctx, data, request);
            retro_sleep(2);
            rect = native_x11_rect(g_x11_dpy,
                  native_x11_window(window));
         } while (     !native_x11_inside(monitors[i], rect)
                    && cpu_features_get_time_usec() < deadline);
         CHECK(native_x11_inside(monitors[i], rect));
         request->settings           = was;
      }
}

/* The worker's lazy start with @spare fds left: none fails its pipe,
 * two its X connection, so it gives up. Either way the screens wait for
 * no list, and the native layer places them as with none. */
static void native_x11_starved(const gfx_ctx_driver_t *ctx, void *data,
      unsigned spare)
{
   video_window_request_t request;
   struct rlimit was, none;
   retro_time_t deadline;
   unsigned n;
   int fd;
   memset(&request, 0, sizeof(request));
   request.screen = 1;
   CHECK(!config_get_ptr()->bools.video_windows_enable);
   CHECK(x11_monitors_serial() == 0 && x11_monitors_state() <= 0);
   window_test_begin();
   window_test_run();
   CHECK(window_test_handed == NULL && window_test_placements() == 2);
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
   /* With no session bus, the D-Bus worker forks dbus-launch at each
    * context start and holds two pipes meanwhile, which can leave a
    * hole below the limit counted next: wait until it has answered */
   dbus_suspend_screensaver(true);
   deadline = cpu_features_get_time_usec() + 3000000;
   while (     dbus_screensaver_state() == DBUS_SCREENSAVER_PENDING
            && cpu_features_get_time_usec() < deadline)
      retro_sleep(1);
   CHECK(dbus_screensaver_state() != DBUS_SCREENSAVER_PENDING);
#endif
   fd = dup(ConnectionNumber(g_x11_dpy));
   if (fd < 0 || getrlimit(RLIMIT_NOFILE, &was) != 0)
   {
      CHECK(!"rlimit");
      if (fd >= 0)
         close(fd);
      window_test_end();
      return;
   }
   close(fd);
   none          = was;
   /* @spare free fds under it, past any open above the lowest free one
    * (Mesa's memfd, made while the D-Bus worker held a pipe) */
   for (none.rlim_cur = (rlim_t)fd, n = 0; n < spare; none.rlim_cur++)
      if (fcntl((int)none.rlim_cur, F_GETFD) == -1)
         n++;
   CHECK(setrlimit(RLIMIT_NOFILE, &none) == 0);
   native_window_drain(ctx, data, &request);
   /* Kept until the worker has failed to connect */
   deadline = cpu_features_get_time_usec() + 3000000;
   while (     spare && x11_monitors_state() == 0
            && cpu_features_get_time_usec() < deadline)
      retro_sleep(1);
   setrlimit(RLIMIT_NOFILE, &was);
   deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      window_test_run();
      if (window_test_handed || cpu_features_get_time_usec() > deadline)
         break;
      native_window_drain(ctx, data, &request);
      retro_sleep(2);
   }
   fprintf(stderr, "native x11: %u fd(s) for the worker: serial %u,"
         " state %d, screen 2 %s\n", spare, x11_monitors_serial(),
         x11_monitors_state(),
         window_test_handed ? "handed" : "in the main window");
   CHECK(     x11_monitors_serial() == 0
           && x11_monitors_state() == (spare ? -1 : 0));
   CHECK(     window_test_handed
           && (window_test_seen.plan.separate_mask & 2u)
           && window_test_seen.settings[1].monitor == 0);
   window_test_end();
}

/* native_x11_starved() with no fd, then with two, each on a context of
 * its own. Returns the context made again, for a worker that starts. */
static const gfx_ctx_driver_t *native_x11_unlisted(
      const gfx_ctx_driver_t *ctx, void **data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   unsigned spare;
   for (spare = 0; spare <= 2; spare += 2)
   {
      native_x11_starved(ctx, *data, spare);
      ctx->destroy(*data);
      *data = NULL;
      if (     !(ctx = video_context_driver_init(false, config_get_ptr(),
                  NULL, ctx, ctx->ident, api, major, minor, false, data))
            || !ctx->set_video_mode(*data, VIDEO_SCALE_PACK(640, 480),
                  false))
      {
         CHECK(!"context again");
         return NULL;
      }
   }
   return ctx;
}

/* vk_x's context does not come up on a grabbed server */
static bool native_x11_grabs;
/* The located steps redo the monitors and grab the server: only on a
 * split made here, with no window manager (it cannot map under a grab) */
static bool native_x11_relayout;

#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
/* The next window made at the origin, on the second head: the one
 * g_x11_screen names is not its own, as when a window manager places
 * it at a first start. @a, @b and @w as for native_x11_two(). Then the
 * server is grabbed, so the worker cannot locate the window before the
 * context publishes. */
static void native_x11_misplaced(int a, int b, int w)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   bool seen             = false;
   native_x11_two(g_x11_dpy, a, b, w);
   while (!seen && cpu_features_get_time_usec() < deadline)
   {
      const x11_monitors_t *snap = x11_monitors_get();
      seen = snap && snap->count == 2 && snap->mon[0].x == a
         && snap->mon[0].w == (unsigned)w;
      x11_monitors_put();
      retro_sleep(2);
   }
   CHECK(seen);
   g_x11_screen = 0;
   if (native_x11_grabs)
      XGrabServer(g_x11_dpy);
   XFlush(g_x11_dpy);
}

/* The toggle on from the start: the screens wait in the main window
 * until the worker has looked for it, so an Auto screen is made once,
 * on the monitor the main window is not on, never on a guess. One on
 * no monitor ends the wait too. Ungrabs the server after 5 frames. */
static void native_x11_made_once(const gfx_ctx_driver_t *ctx, void *data,
      const char *step, bool unlocated)
{
   video_window_request_t request;
   native_rect_t monitors[8];
   native_rect_t rect;
   retro_time_t deadline;
   const x11_monitors_t *snap;
   unsigned i, first, again, expect;
   unsigned waited = 0;
   int own         = -1;
   bool unknown    = false;
   unsigned count  = native_x11_monitors(g_x11_dpy, monitors, 8);
   memset(&request, 0, sizeof(request));
   request.screen = 1;
   rect           = native_x11_rect(g_x11_dpy, g_x11_win);
   for (i = 0; i < count; i++)
      if (native_x11_inside(monitors[i], rect))
         own = (int)i;
   /* On none: the list names the head it was made for */
   expect = (own >= 0 ? (unsigned)own : g_x11_screen) == 0 ? 2 : 1;
   CHECK(unlocated ? own < 0 : (own >= 0 && (unsigned)own != g_x11_screen));
   CHECK(config_get_ptr()->bools.video_windows_enable);
   window_test_begin();
   deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      window_test_run();
      if (window_test_handed && (window_test_seen.plan.separate_mask & 2u))
         break;
      CHECK(window_test_placements() == 2);
      if (cpu_features_get_time_usec() > deadline)
         break;
      if (++waited == 5)
      {
         XUngrabServer(g_x11_dpy);
         XFlush(g_x11_dpy);
      }
      native_window_drain(ctx, data, &request);
      retro_sleep(2);
   }
   XUngrabServer(g_x11_dpy);
   XFlush(g_x11_dpy);
   first = window_test_handed ? window_test_seen.settings[1].monitor : 0;
   again = first;
   for (i = 0; i < 25 && again == first; i++)
   {
      native_window_drain(ctx, data, &request);
      retro_sleep(2);
      window_test_run();
      again = window_test_handed ? window_test_seen.settings[1].monitor : 0;
   }
   snap    = x11_monitors_get();
   unknown = snap && snap->own < 0 && snap->win == g_x11_win;
   x11_monitors_put();
   fprintf(stderr, "native x11: %s: main window on monitor %d, screen 2"
         " made after %u frame(s) on monitor %u, then on %u\n", step,
         own + 1, waited, first, again);
   CHECK(first == expect);
   CHECK(again == first);
   if (unlocated)
      CHECK(unknown);
   window_test_end();
}
#endif

/* Two monitors: the start native_x11_torn_down() set up, a reinit, and
 * a reinit onto no monitor, each through native_x11_made_once().
 * Returns the context made again. */
static const gfx_ctx_driver_t *native_x11_located(
      const gfx_ctx_driver_t *ctx, void **data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_request_t request;
   native_rect_t monitors[8];
   unsigned i, n;
   int half       = DisplayWidth(g_x11_dpy, DefaultScreen(g_x11_dpy)) / 2;
   unsigned heads = native_x11_heads(g_x11_dpy);
   if (heads < 2)
      return ctx;
   /* Before anything pumped: the init publish, not the foreign list */
   n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   fprintf(stderr, "native x11: %u monitor(s) published at init%s\n", n,
         n && string_is_equal(got[0].name, "foreign")
         ? ", the foreign list" : "");
   native_x11_monitors(g_x11_dpy, monitors, 8);
   CHECK(native_x11_monitors_match(monitors, heads, -1, got, n));
   if (!native_x11_relayout)
   {
      fprintf(stderr, "native x11: the located steps not run: they need"
            " the NATIVE_X11_MONITORS=2 split and no window manager\n");
      return ctx;
   }
   for (i = 0; i < 3; i++)
   {
      if (i)
      {
         config_get_ptr()->bools.video_windows_enable = true;
         ctx->destroy(*data);
         *data = NULL;
         /* The last: both right of the origin */
         if (i == 2)
            native_x11_misplaced(half, half + half / 2, half / 2);
         else
            native_x11_misplaced(half, 0, half);
         if (     !(ctx = video_context_driver_init(false, config_get_ptr(),
                     NULL, ctx, ctx->ident, api, major, minor, false, data))
               || !ctx->set_video_mode(*data, VIDEO_SCALE_PACK(640, 480),
                     false))
         {
            CHECK(!"context again");
            return NULL;
         }
      }
      native_x11_made_once(ctx, *data, i == 0 ? "start"
            : i == 1 ? "reinit" : "on no monitor", i == 2);
   }
   native_x11_two(g_x11_dpy, 0, half, half);
   memset(&request, 0, sizeof(request));
   request.screen = 1;
   native_x11_monitors_wait(ctx, *data, &request, monitors,
         native_x11_monitors(g_x11_dpy, monitors, 8), -1, got);
#endif
   return ctx;
}

#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
/* Takes away every monitor the fixture named */
static void native_x11_unlay(Display *dpy)
{
   int i, n             = 0;
   XRRMonitorInfo *mons = XRRGetMonitors(dpy, DefaultRootWindow(dpy),
         False, &n);
   for (i = 0; mons && i < n; i++)
   {
      char *name = XGetAtomName(dpy, mons[i].name);
      if (name && strncmp(name, "RA-", 3) == 0)
         XRRDeleteMonitor(dpy, DefaultRootWindow(dpy), mons[i].name);
      if (name)
         XFree(name);
   }
   if (mons)
      XRRFreeMonitors(mons);
   XSync(dpy, False);
}

/* RA-A with the output, RA-M and RA-B, side by side, each @w wide */
static void native_x11_three(Display *dpy, int w)
{
   RROutput output         = 0;
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy,
         DefaultRootWindow(dpy));
   if (res)
   {
      if (res->noutput > 0)
         output = res->outputs[0];
      XRRFreeScreenResources(res);
   }
   native_x11_rr_set(dpy, "RA-A", 0, w, output);
   native_x11_rr_set(dpy, "RA-M", w, w, 0);
   native_x11_rr_set(dpy, "RA-B", 2 * w, w, 0);
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_ENUMERATE);
}

/* Takes monitor @name away and has the worker look */
static void native_x11_unplug(Display *dpy, const char *name)
{
   XRRDeleteMonitor(dpy, DefaultRootWindow(dpy),
         XInternAtom(dpy, name, False));
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_ENUMERATE);
}

/* Until the worker has @want monitors: their bounds and ordinals */
static bool native_x11_snap(unsigned want, native_rect_t *areas,
      unsigned *ordinals)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   unsigned i, n         = 0;
   for (;;)
   {
      const x11_monitors_t *snap = x11_monitors_get();
      n = snap ? snap->count : 0;
      for (i = 0; snap && i < n && i < 8; i++)
      {
         areas[i].x  = snap->mon[i].x;
         areas[i].y  = snap->mon[i].y;
         areas[i].w  = snap->mon[i].w;
         areas[i].h  = snap->mon[i].h;
         ordinals[i] = snap->mon[i].ordinal;
      }
      x11_monitors_put();
      if (n == want || cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   CHECK(n == want);
   return n == want;
}

/* The ordinal of the monitor at @x, 0 for none */
static unsigned native_x11_ordinal_at(const native_rect_t *areas,
      const unsigned *ordinals, unsigned n, int x)
{
   unsigned i;
   for (i = 0; i < n; i++)
      if (areas[i].x == x)
         return ordinals[i];
   return 0;
}

/* Pumps every screen's windows for @usec: the GEOMETRY events that
 * save */
static unsigned native_x11_pumped(const gfx_ctx_driver_t *ctx, void *data,
      retro_time_t usec)
{
   video_window_event_t events[4];
   unsigned i, count;
   unsigned saves        = 0;
   retro_time_t deadline = cpu_features_get_time_usec() + usec;
   do
   {
      unsigned dims = VIDEO_SCALE_PACK(640, 480);
      bool quit     = false;
      bool resize   = false;
      ctx->check_window(data, &quit, &resize, &dims);
      while ((count = ctx->windows->poll(data, events,
                  sizeof(events) / sizeof(*events))))
         for (i = 0; i < count; i++)
            if (     events[i].kind == VIDEO_WINDOW_EVENT_GEOMETRY
                  && events[i].save_dims)
               saves++;
      retro_sleep(2);
   } while (cpu_features_get_time_usec() < deadline);
   return saves;
}

/* Pumps every screen's windows until @n monitors are published, the
 * main window on the @own'th (-1: any): the GEOMETRY events that save */
static unsigned native_x11_published(const gfx_ctx_driver_t *ctx,
      void *data, unsigned n, int own)
{
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   unsigned saves        = 0;
   for (;;)
   {
      unsigned count = video_driver_get_window_displays(got,
            VIDEO_WINDOW_DISPLAYS_MAX);
      if (count == n && (own < 0 || (own < (int)count && got[own].own)))
         return saves;
      if (cpu_features_get_time_usec() > deadline)
         break;
      saves += native_x11_pumped(ctx, data, 5000);
   }
   CHECK(!"monitors published");
   return saves;
}

/* Where the window's record has it, by its last ConfigureNotify */
static native_rect_t native_x11_record(const video_window_native_t *window)
{
   native_rect_t rect;
   const x11_video_window_t *state =
      (const x11_video_window_t*)window->state;
   rect.x = state ? VIDEO_POS_X(state->common.pos) : 0;
   rect.y = state ? VIDEO_POS_Y(state->common.pos) : 0;
   rect.w = VIDEO_SCALE_W(window->dims);
   rect.h = VIDEO_SCALE_H(window->dims);
   return rect;
}

/* The monitor of @n that has all of @rect, -1 for none */
static int native_x11_on(const native_rect_t *areas, unsigned n,
      native_rect_t rect)
{
   unsigned i;
   for (i = 0; i < n; i++)
      if (native_x11_inside(areas[i], rect))
         return (int)i;
   return -1;
}

/* A monitor taken away: each window it leaves on none is placed again,
 * whatever monitor it prefers, and that move is not saved; a window on
 * a monitor that stays does not move. Then the middle one of three,
 * whose window's monitor number the shorter list still has. On the
 * split, which it puts back. */
static void native_x11_gone(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   static bool done;
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_request_t req[4];
   video_window_native_t win[4];
   native_rect_t areas[8], monitors[8], was, rect;
   unsigned ordinals[8];
   unsigned i, saves, b, m, count;
   int on;
   int width = DisplayWidth(g_x11_dpy, DefaultScreen(g_x11_dpy));
   int half  = width / 2;
   int third = width / 3;
   Display *fix;

   if (done)
      return;
   done = true;
   if (!native_x11_relayout)
   {
      fprintf(stderr, "native x11: monitors taken away not checked: that"
            " needs the NATIVE_X11_MONITORS=2 split and no window"
            " manager\n");
      return;
   }
   if (!(fix = XOpenDisplay(DisplayString(g_x11_dpy))))
   {
      CHECK(!"fixture display");
      return;
   }
   ctx->windows->destroy(data, window);
   memset(win, 0, sizeof(win));

   /* Saved on RA-B with no preferred monitor; preferring RA-A;
    * fullscreen on RA-B; preferring RA-B */
   native_x11_unlay(fix);
   native_x11_two(fix, 0, half, half);
   native_x11_snap(2, areas, ordinals);
   b = native_x11_ordinal_at(areas, ordinals, 2, half);
   for (i = 0; i < 4; i++)
   {
      req[i]                     = *request;
      req[i].screen              = i + 1;
      req[i].generation          = video_driver_window_generation();
      req[i].settings.monitor    = 0;
      req[i].settings.fullscreen = false;
      req[i].settings.pos        = VIDEO_POS_PACK(half + 140, 100);
      req[i].settings.dims       = VIDEO_SCALE_PACK(400, 300);
      req[i].settings.pos_set    = true;
   }
   req[1].settings.monitor    = native_x11_ordinal_at(areas, ordinals, 2, 0);
   req[1].settings.pos        = VIDEO_POS_PACK(100, 100);
   req[2].settings.fullscreen = true;
   req[3].settings.monitor    = b;
   req[3].settings.pos        = VIDEO_POS_PACK(half + 140, 500);
   for (i = 0; i < 4; i++)
      CHECK(ctx->windows->create(data, &req[i], &win[i]));
   native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   for (i = 0; i < 4; i++)
      CHECK(win[i].presentable);
   CHECK(native_x11_record(&win[0]).x >= half);
   was = native_x11_rect(fix, native_x11_window(&win[1]));

   native_x11_unplug(fix, "RA-B");
   native_x11_snap(1, areas, ordinals);
   saves = native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   for (i = 0; i < 4; i++)
   {
      rect = native_x11_record(&win[i]);
      fprintf(stderr, "native x11: RA-B gone, window %u (monitor %u%s)"
            " at %d,%d %ux%u\n", i + 1, req[i].settings.monitor,
            req[i].settings.fullscreen ? ", fullscreen" : "",
            rect.x, rect.y, rect.w, rect.h);
   }
   CHECK(native_x11_inside(areas[0], native_x11_record(&win[0])));
   CHECK(native_x11_inside(areas[0],
            native_x11_rect(fix, native_x11_window(&win[0]))));
   rect = native_x11_rect(fix, native_x11_window(&win[1]));
   CHECK(rect.x == was.x && rect.y == was.y
         && rect.w == was.w && rect.h == was.h);
   rect = native_x11_rect(fix, native_x11_window(&win[2]));
   CHECK(rect.x == areas[0].x && rect.y == areas[0].y
         && rect.w == areas[0].w && rect.h == areas[0].h);
   CHECK(native_x11_inside(areas[0], native_x11_record(&win[3])));
   CHECK(saves == 0);
   for (i = 0; i < 4; i++)
      ctx->windows->destroy(data, &win[i]);

   /* Three, preferring the middle one */
   native_x11_unlay(fix);
   native_x11_three(fix, third);
   native_x11_snap(3, areas, ordinals);
   m                          = native_x11_ordinal_at(areas, ordinals, 3,
         third);
   req[0].generation          = video_driver_window_generation();
   req[0].settings.monitor    = m;
   req[0].settings.fullscreen = false;
   req[0].settings.pos        = VIDEO_POS_PACK(third + 50, 100);
   req[0].settings.dims       = VIDEO_SCALE_PACK(300, 200);
   CHECK(ctx->windows->create(data, &req[0], &win[0]));
   native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(native_x11_record(&win[0]).x >= third
         && native_x11_record(&win[0]).x < 2 * third);
   native_x11_unplug(fix, "RA-M");
   native_x11_snap(2, areas, ordinals);
   saves = native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   rect  = native_x11_record(&win[0]);
   on    = native_x11_on(areas, 2, rect);
   fprintf(stderr, "native x11: RA-M (monitor %u of 3) gone, its window"
         " at %d,%d %ux%u, on monitor %u of 2\n", m, rect.x, rect.y,
         rect.w, rect.h, on >= 0 ? ordinals[on] : 0);
   CHECK(on >= 0);
   CHECK(native_x11_on(areas, 2,
            native_x11_rect(fix, native_x11_window(&win[0]))) >= 0);
   CHECK(saves == 0);
   ctx->windows->destroy(data, &win[0]);

   /* What a window manager does as a monitor goes is not saved, and the
    * user's move after the settle is */
   native_x11_unlay(fix);
   native_x11_two(fix, 0, half, half);
   req[0].generation       = video_driver_window_generation();
   req[0].settings.monitor = 0;
   req[0].settings.pos     = VIDEO_POS_PACK(100, 100);
   req[0].settings.dims    = VIDEO_SCALE_PACK(400, 300);
   CHECK(ctx->windows->create(data, &req[0], &win[0]));
   native_x11_published(ctx, data, 2, -1);
   native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_x11_unplug(fix, "RA-B");
   saves  = native_x11_published(ctx, data, 1, -1);
   XMoveWindow(fix, native_x11_window(&win[0]), 150, 120);
   XFlush(fix);
   saves += native_x11_pumped(ctx, data, 200000);
   rect   = native_x11_record(&win[0]);
   fprintf(stderr, "native x11: moved as RA-B went, to %d,%d, %u save(s)\n",
         rect.x, rect.y, saves);
   CHECK(rect.x == 150 && rect.y == 120);
   CHECK(saves == 0);
   native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC);
   XMoveWindow(fix, native_x11_window(&win[0]), 200, 150);
   XFlush(fix);
   CHECK(native_x11_pumped(ctx, data, 200000) == 1);

   /* The same monitors, the main window on another: a window the user
    * left on none stays */
   native_x11_unlay(fix);
   native_x11_two(fix, 0, half, half);
   native_x11_published(ctx, data, 2, -1);
   native_x11_pumped(ctx, data, VIDEO_WINDOW_SETTLE_USEC + 100000);
   XMoveWindow(fix, native_x11_window(&win[0]), width + 50, 100);
   XFlush(fix);
   native_x11_pumped(ctx, data, 200000);
   was = native_x11_rect(fix, g_x11_win);
   XMoveWindow(fix, g_x11_win, was.x < half ? half + 100 : 100, 100);
   XFlush(fix);
   native_x11_published(ctx, data, 2, was.x < half ? 1 : 0);
   native_x11_pumped(ctx, data, 200000);
   rect = native_x11_record(&win[0]);
   fprintf(stderr, "native x11: main window to the other monitor, the"
         " window on none at %d,%d\n", rect.x, rect.y);
   CHECK(rect.x == width + 50 && rect.y == 100);
   XMoveWindow(fix, g_x11_win, was.x, was.y);
   XFlush(fix);
   native_x11_published(ctx, data, 2, was.x < half ? 0 : 1);
   ctx->windows->destroy(data, &win[0]);

   native_x11_unlay(fix);
   native_x11_two(fix, 0, half, half);
   XCloseDisplay(fix);
   count = native_x11_monitors(g_x11_dpy, monitors, 8);
   native_x11_monitors_wait(ctx, data, request, monitors, count, -1, got);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}
#endif
#endif

/* The monitors as the screens' Display setting lists them, republished
 * when one changes or the main window goes to another, and only then */
static void native_x11_monitors_check(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request, Display *display,
      const native_rect_t *monitors)
{
#ifdef RARCH_HAVE_X11_MONITORS
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned i, n, restore, retry, serial;
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned want                  = native_x11_heads(display);
   int from                       = -1;
   n = native_x11_monitors_wait(ctx, data, request, monitors, want, -1,
         got);
   for (i = 0; i < n; i++)
   {
      char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      CHECK(got[i].valid && got[i].on && got[i].physical && got[i].monitor);
      if (got[i].own)
         from = (int)i;
      video_windows_display_key(got, i, key, sizeof(key));
      fprintf(stderr, "native x11: monitor %u \"%s\"%s\n",
            got[i].ordinal, key, got[i].own ? ", main window" : "");
      CHECK(key[0] != '\0');
   }
   if (n >= 2)
      CHECK(     string_is_equal(got[0].name, "RA-A")
              && string_is_equal(got[1].name, "RA-B"));

   /* Located again and again: the worker publishes, the list does not
    * change, nothing is republished */
   native_x11_located_wait();
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   restore = video_st->windows_restore;
   retry   = video_st->windows_retry;
   serial  = x11_monitors_serial();
   x11_monitors_window(0);
   native_x11_serial_wait(serial);
   native_window_settle(ctx, data, request, 50000);
   serial  = x11_monitors_serial();
   x11_monitors_window(g_x11_win);
   native_x11_serial_wait(serial);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(video_st->windows_retry == retry);

   /* The main window's monitor follows it */
   if (n >= 2 && from >= 0)
   {
      native_rect_t was = native_x11_rect(display, g_x11_win);
      int to            = from ? 0 : 1;
      XMoveWindow(display, g_x11_win, monitors[to].x + 100,
            monitors[to].y + 100);
      XFlush(display);
      native_x11_monitors_wait(ctx, data, request, monitors, want, to, got);
      CHECK(video_st->windows_retry != retry);
      CHECK(video_st->windows_restore == restore);
      XMoveWindow(display, g_x11_win, was.x, was.y);
      XFlush(display);
      native_x11_monitors_wait(ctx, data, request, monitors, want, from,
            got);
   }
   else
      fprintf(stderr, "native x11: the main window going to another"
            " monitor not checked\n");
#endif
}

#ifdef RARCH_HAVE_X11_MONITORS
static bool native_x11_foreign_up;
#endif

/* A context gone takes its list with it; a foreign one put up after
 * must be replaced by the next context's */
static void native_x11_torn_down(bool again)
{
#ifdef RARCH_HAVE_X11_MONITORS
   static video_window_display_t foreign[1];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 0);
   if (!again)
      return;
   memset(foreign, 0, sizeof(foreign));
   foreign[0].ordinal = 1;
   foreign[0].valid   = foreign[0].on = foreign[0].physical = true;
   foreign[0].monitor = true;
   strlcpy(foreign[0].name, "foreign", sizeof(foreign[0].name));
   video_driver_publish_window_displays(foreign, 1);
   native_x11_foreign_up = true;
   /* On at the next connect, which publishes the list at init */
   config_get_ptr()->bools.video_windows_enable = true;
#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
   if (native_x11_relayout && native_x11_heads(g_x11_dpy) >= 2)
   {
      int half = DisplayWidth(g_x11_dpy, DefaultScreen(g_x11_dpy)) / 2;
      native_x11_misplaced(half, 0, half);
   }
#endif
#endif
}

static const gfx_ctx_driver_t *native_x11_context(const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor, void **data)
{
   const gfx_ctx_driver_t *ctx;
#if defined(HAVE_VULKAN) && defined(HAVE_XCB)
   if (strcmp(ident, "vk_x") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_vk_x, ident, api, major, minor, false, data);
   else
#endif
   ctx = video_context_driver_init_first(NULL, ident, api, major, minor,
         false, data);
   if (     !ctx
         || !ctx->set_video_mode(*data, VIDEO_SCALE_PACK(640, 480), false))
   {
      CHECK(!"context");
      return NULL;
   }
   if (api == GFX_CTX_OPENGL_API)
      ctx->swap_interval(*data, 1);
   return ctx;
}

/* Pumps until the keyboard focus is on 'want' */
static Window native_x11_focus_on(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, Display *display, Window want)
{
   Window focus          = None;
   int revert            = 0;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      XGetInputFocus(display, &focus, &revert);
      if (focus == want || cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   native_window_settle(ctx, data, request, 300000);
   XGetInputFocus(display, &focus, &revert);
   return focus;
}

/* A key the user pressed on the main window, at the server's time as
 * x11_alive() reads it, then a moment: what RetroArch does next is
 * later than the user's last input */
static void native_x11_user_key(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, Display *display)
{
   XEvent event;
   Time now;
   Window clock = XCreateSimpleWindow(display, DefaultRootWindow(display),
         0, 0, 1, 1, 0, 0, 0);
   XSelectInput(display, clock, PropertyChangeMask);
   XChangeProperty(display, clock, XA_WM_NAME, XA_STRING, 8,
         PropModeReplace, (const unsigned char*)"t", 1);
   XWindowEvent(display, clock, PropertyChangeMask, &event);
   now = event.xproperty.time;
   XDestroyWindow(display, clock);
   memset(&event, 0, sizeof(event));
   event.xkey.type        = KeyPress;
   event.xkey.window      = g_x11_win;
   event.xkey.root        = DefaultRootWindow(display);
   event.xkey.time        = now;
   event.xkey.same_screen = True;
   event.xkey.keycode     = XKeysymToKeycode(display, XStringToKeysym("Shift_L"));
   XSendEvent(display, g_x11_win, False, KeyPressMask, &event);
   event.xkey.type        = KeyRelease;
   XSendEvent(display, g_x11_win, False, KeyReleaseMask, &event);
   XSync(display, False);
   native_window_settle(ctx, data, request, 100000);
}

/* The main window remade, as its fullscreen toggle remakes it on X11,
 * with a screen's window up: the driver's teardown keeps that window,
 * the next driver's start takes it, and nothing is asked of it. The
 * focus stays on the window the user left it on: the main window, then
 * the screen's, which first goes fullscreen and back in place. Last,
 * the screen's again, taken and gone at once as when its drawable
 * cannot be made: the focus stays on the main window. */
static void native_x11_kept(const char *ident, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   unsigned round;
   for (round = 0; round < 3; round++)
   {
      video_window_request_t request;
      video_window_native_t window;
      native_rect_t rect, main_rect;
      XEvent event;
      Window win, focus;
      Display *watch;
      VisualID visual;
      unsigned display = 0;
      unsigned asked   = 0;
      void *data       = NULL;
      char *wm;
      bool managed;
      const gfx_ctx_driver_t *ctx = native_x11_context(ident, api, major,
            minor, &data);
      if (!ctx)
         return;
      wm      = x11_get_wm_name(g_x11_dpy);
      managed = wm != NULL;
      free(wm);
      memset(&request, 0, sizeof(request));
      memset(&window, 0, sizeof(window));
      request.screen        = 1;
      request.generation    = video_driver_window_generation();
      request.settings.mode = VIDEO_WINDOW_SEPARATE;
      request.settings.dims = VIDEO_SCALE_PACK(320, 240);
      CHECK(ctx->windows->create(data, &request, &window));
      native_window_wait(ctx, data, &request, &window,
            NATIVE_EVENT(RESUME), 0);
      native_window_settle(ctx, data, &request, 100000);
      win = native_x11_window(&window);
      if (round)
      {
         native_x11_focused(ctx, data, &request, &window, g_x11_dpy, win,
               managed);
         main_rect                   = native_x11_rect(g_x11_dpy, g_x11_win);
         request.settings.fullscreen = true;
         ctx->windows->update(data, &window, &request);
         if (managed)
            native_x11_wait_fullscreen(ctx, data, &request, &window, true);
         request.settings.fullscreen = false;
         ctx->windows->update(data, &window, &request);
         if (managed)
            native_x11_wait_fullscreen(ctx, data, &request, &window, false);
         native_window_settle(ctx, data, &request, 300000);
         rect = native_x11_rect(g_x11_dpy, g_x11_win);
         CHECK(native_x11_focus_on(ctx, data, &request, g_x11_dpy, win)
               == win);
         CHECK(     rect.x == main_rect.x && rect.y == main_rect.y
                 && rect.w == main_rect.w && rect.h == main_rect.h);
      }
      else
      {
         native_x11_focus(g_x11_dpy, g_x11_win, managed);
         CHECK(native_x11_focus_on(ctx, data, &request, g_x11_dpy,
               g_x11_win) == g_x11_win);
      }
      CHECK(window.focused == (round != 0));
      rect   = native_x11_rect(g_x11_dpy, win);
      visual = ((x11_video_window_t*)window.state)->visual;

      CHECK((watch = XOpenDisplay(NULL)) != NULL);
      if (!watch)
         return;
      XSelectInput(watch, win, StructureNotifyMask);
      XSync(watch, False);
      if (round)
         native_x11_user_key(ctx, data, &request, watch);

      if (ctx->windows->keep)
         ctx->windows->keep(data, &window);
      else
         ctx->windows->destroy(data, &window);
      CHECK(window.state == NULL);
      ctx->destroy(data);
      data = NULL;
      /* A context up and gone in between, as a probe that fails is */
      if (round && (data = ctx->init(NULL)))
         ctx->destroy(data);
      data = NULL;
      if (!(ctx = native_x11_context(ident, api, major, minor, &data)))
      {
         XCloseDisplay(watch);
         return;
      }
      if (round == 2)
      {
         CHECK(x11_video_window_take(data, request.screen, &window,
               &display, visual));
         x11_video_window_destroy(data, &window);
         focus = native_x11_focus_on(ctx, data, &request, g_x11_dpy,
               g_x11_win);
         fprintf(stderr, "native x11: taken and gone at once, focus on "
               "%s\n", focus == g_x11_win ? "main" : "another");
         CHECK(focus == g_x11_win);
         XCloseDisplay(watch);
         ctx->destroy(data);
         continue;
      }
      if (     !ctx->windows->take
            || !ctx->windows->take(data, request.screen, &window, &display))
      {
         CHECK(ctx->windows->create(data, &request, &window));
         native_window_wait(ctx, data, &request, &window,
               NATIVE_EVENT(RESUME), 0);
      }
      focus = native_x11_focus_on(ctx, data, &request, g_x11_dpy,
            round ? win : g_x11_win);
      fprintf(stderr, "native x11: remade main, screen focused %d: window "
            "%s, focus on %s\n", round, native_x11_window(&window) == win
            ? "kept" : "made again", focus == g_x11_win ? "main"
            : focus == native_x11_window(&window) ? "the screen's"
            : "another");
      CHECK(native_x11_window(&window) == win);
      CHECK(focus == (round ? win : g_x11_win));
      CHECK(window.focused == (round == 1));
      CHECK(window.generation == request.generation);
      XSync(watch, False);
      while (XPending(watch))
      {
         XNextEvent(watch, &event);
         if (     event.xany.window == win
               && (     event.type == UnmapNotify
                     || event.type == DestroyNotify
                     || event.type == MapNotify))
            asked++;
      }
      CHECK(asked == 0);
      if (native_x11_window(&window) == win)
      {
         native_rect_t now = native_x11_rect(g_x11_dpy, win);
         CHECK(     now.x == rect.x && now.y == rect.y
                 && now.w == rect.w && now.h == rect.h);
      }
      XCloseDisplay(watch);
      ctx->windows->destroy(data, &window);
      ctx->destroy(data);
   }
}

static void native_x11_lifecycle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   native_rect_t monitors[8];
   native_rect_t rect;
   video_window_request_t other;
   video_window_native_t invalid;
   XEvent event;
   unsigned i, seen, last;
   int (*handler)(Display*, XErrorEvent*);
   Display *display = g_x11_dpy;
   Window primary   = g_x11_win;
   Window secondary = native_x11_window(window);
   char *wm         = x11_get_wm_name(display);
   bool managed     = wm != NULL;
#ifdef RARCH_HAVE_X11_MONITORS
   bool gate        = true;
#endif
   unsigned count;

#ifdef RARCH_HAVE_X11_MONITORS
   if (native_x11_split(display))
      native_x11_relayout = !managed;
   native_x11_grabs = !string_is_equal(ctx->ident, "vk_x");
#endif
   count = native_x11_monitors(display, monitors, 8);
   fprintf(stderr, "native x11: %u monitor(s), window manager %s\n",
         count, wm ? wm : "none");
   free(wm);
#ifdef RARCH_HAVE_X11_MONITORS
   /* The worker kept its snapshot: published at init, before anything
    * pumped, in place of the foreign list */
   if (native_x11_foreign_up)
   {
      video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
      unsigned n = video_driver_get_window_displays(got,
            VIDEO_WINDOW_DISPLAYS_MAX);
      native_x11_foreign_up = false;
      fprintf(stderr, "native x11: %u monitor(s) published at init%s\n",
            n, n && string_is_equal(got[0].name, "foreign")
            ? ", the foreign list" : "");
      CHECK(native_x11_monitors_match(monitors, native_x11_heads(display),
            -1, got, n));
      config_get_ptr()->bools.video_windows_enable = false;
      gate = false;
   }
#endif
   CHECK(display == g_x11_dpy);
   CHECK(secondary != None && secondary != primary);
   CHECK(native_x11_titled(display, video_window_title_screen(request)) == 1);
   native_x11_titles(display, primary, secondary,
         video_window_title_screen(request));
   /* After a restart, what was queued for the last lifetime's window
    * (a close and a resize) has to reach nothing */
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 100000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->dims == request->settings.dims);
   CHECK(window->configured && window->presentable);
   CHECK(native_x11_primary_intact(display, primary));
#ifdef RARCH_HAVE_X11_MONITORS
   /* The first lifetime's: the next connects with the toggle on */
   if (gate)
      native_x11_worker_gate(ctx, data, request, window, monitors);
#endif
   native_x11_monitors_check(ctx, data, request, display, monitors);

   /* Resized by the user; the events follow where the window is */
   XResizeWindow(display, secondary, 400, 300);
   XFlush(display);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
         VIDEO_SCALE_PACK(400, 300));
   memset(&rect, 0, sizeof(rect));
   native_x11_wait_rect(ctx, data, request, window, rect);
   CHECK(window->dims == VIDEO_SCALE_PACK(400, 300));

   native_x11_preferred(ctx, data, request, window, monitors, count);

   /* No preferred monitor, and saved bounds on none: the main window's */
   request->settings.monitor = 0;
   request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, native_x11_centred(
         monitors[g_x11_screen < count ? g_x11_screen : 0], 320, 240));

   /* A saved monitor that is gone: the main window's instead */
   native_x11_preferred(ctx, data, request, window, monitors, count);
   request->settings.monitor = count + 3;
   request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, native_x11_centred(
         monitors[g_x11_screen < count ? g_x11_screen : 0], 320, 240));

   /* Moved by the user onto the last monitor */
   rect    = native_x11_rect(display, secondary);
   rect.x  = monitors[count - 1].x + 40;
   rect.y  = monitors[count - 1].y + 40;
   XMoveWindow(display, secondary, rect.x, rect.y);
   XFlush(display);
   native_x11_wait_rect(ctx, data, request, window, rect);

   /* Borderless fullscreen where it is, then on a preferred monitor;
    * where it is counts whether a position was set or not */
   request->settings.monitor    = 0;
   request->settings.fullscreen = true;
   request->settings.pos_set    = false;
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, monitors[count - 1]);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, request, window, true);
   request->settings.monitor    = 1;
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, monitors[0]);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, request, window, true);

   /* A window again, at a saved origin */
   request->settings.fullscreen = false;
   request->settings.monitor    = 0;
   request->settings.pos        = VIDEO_POS_PACK(monitors[0].x + 50,
         monitors[0].y + 50);
   request->settings.pos_set    = true;
   ctx->windows->update(data, window, request);
   rect.x = monitors[0].x + 50;
   rect.y = monitors[0].y + 50;
   rect.w = 320;
   rect.h = 240;
   native_x11_wait_rect(ctx, data, request, window, rect);
   if (managed)
      native_x11_wait_fullscreen(ctx, data, request, window, false);
   CHECK(native_x11_primary_intact(display, primary));

   /* The same origin, never set: centred on the main window's monitor */
   request->settings.pos_set    = false;
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, native_x11_centred(
         monitors[g_x11_screen < count ? g_x11_screen : 0], 320, 240));
   request->settings.pos_set    = true;
   ctx->windows->update(data, window, request);
   native_x11_wait_rect(ctx, data, request, window, rect);

   /* Automatic bounds: the main window's size, centred on its monitor
    * whatever the saved origin */
   {
      native_rect_t main_rect = native_x11_rect(display, primary);
      request->settings.dims  = 0;
      ctx->windows->update(data, window, request);
      native_x11_wait_rect(ctx, data, request, window, native_x11_centred(
            monitors[g_x11_screen < count ? g_x11_screen : 0],
            main_rect.w, main_rect.h));
      request->settings.dims  = VIDEO_SCALE_PACK(320, 240);
      ctx->windows->update(data, window, request);
      native_x11_wait_rect(ctx, data, request, window, rect);
   }

   /* A request of an older generation changes nothing */
   native_window_settle(ctx, data, request, 100000);
   other                = *request;
   other.generation     = request->generation - 1;
   other.settings.pos   = VIDEO_POS_PACK(monitors[0].x + 200,
         monitors[0].y + 200);
   other.settings.dims  = VIDEO_SCALE_PACK(200, 150);
   ctx->windows->update(data, window, &other);
   seen = native_window_settle(ctx, data, request, 200000);
   CHECK(!(seen & NATIVE_EVENT(GEOMETRY)));
   CHECK(window->dims == VIDEO_SCALE_PACK(320, 240));

   /* Withdrawn and shown again, then minimised and restored */
   XUnmapWindow(display, secondary);
   XFlush(display);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(SUSPEND), 0);
   CHECK(!window->presentable);
   XMapWindow(display, secondary);
   XFlush(display);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(window->presentable);
   if (managed)
   {
      XIconifyWindow(display, secondary, DefaultScreen(display));
      XFlush(display);
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(SUSPEND), 0);
      CHECK(!window->presentable);
      XMapWindow(display, secondary);
      XFlush(display);
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
      CHECK(window->presentable);
   }

   native_gl_binding(ctx, data, window);
   CHECK(native_x11_primary_intact(display, primary));

   /* Close and focus loss outlast a flood the consumer has no room for */
   memset(&event, 0, sizeof(event));
   event.xfocus.type   = FocusOut;
   event.xfocus.window = secondary;
   event.xfocus.mode   = NotifyNormal;
   event.xfocus.detail = NotifyNonlinear;
   for (i = 0; i < 128; i++)
      XSendEvent(display, secondary, False, FocusChangeMask, &event);
   native_x11_close(display, secondary);
   XSync(display, False);
   {
      unsigned dims = VIDEO_SCALE_PACK(640, 480);
      bool quit     = false;
      bool resize   = false;
      ctx->check_window(data, &quit, &resize, &dims);
      CHECK(!quit);
   }
   CHECK(ctx->windows->poll(data, NULL, 0) == 0);
   for (i = 0, seen = 0; i < 16; i++)
   {
      video_window_event_t one;
      if (!ctx->windows->poll(data, &one, 1))
         break;
      CHECK(one.screen == request->screen);
      CHECK(one.generation == request->generation);
      CHECK(one.user == (one.kind == VIDEO_WINDOW_EVENT_CLOSE));
      seen |= 1u << one.kind;
   }
   CHECK(i <= 6);
   CHECK(seen & NATIVE_EVENT(CLOSE));
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->presentable && !window->focused);
   CHECK(native_x11_primary_intact(display, primary));
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));

   /* What is still queued for the closed window cannot reach the one
    * that replaces it */
   XResizeWindow(display, secondary, 500, 400);
   native_x11_close(display, secondary);
   XSync(display, False);
   last = request->generation;
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL && window->surface == NULL);
   request->generation = video_driver_window_generation();
   CHECK(request->generation > last);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(window->generation == request->generation);
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->presentable);
   CHECK(window->dims == request->settings.dims);
   secondary = native_x11_window(window);

   /* Focus going to a screen's window stays RetroArch's. Closing the
    * focused window takes its focus with it. */
   native_x11_has_focus(ctx, data, request, display, primary, managed);
   native_x11_focused(ctx, data, request, window, display, secondary,
         managed);
   CHECK(ctx->has_focus(data));
   native_x11_keys(ctx, data, request, display, secondary);
   native_x11_close(display, secondary);
   XSync(display, False);
   seen = native_window_drain(ctx, data, request);
   CHECK(seen & NATIVE_EVENT(CLOSE));
   CHECK(native_window_closed_user);
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->focused && !window->presentable);
   CHECK(!ctx->has_focus(data));
   ctx->windows->destroy(data, window);
   /* The window handler hands on to the one it replaced, counted here */
   XSetErrorHandler(native_x11_count_error);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   native_x11_errors_elsewhere(display, native_x11_window(window));
   ctx->windows->destroy(data, window);
   /* Xlib's default handler back, as a save and restore elsewhere can
    * leave it: the next create has to put the window handler back */
   XSetErrorHandler(NULL);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = native_x11_window(window);

   /* ... and so does destroying it */
   native_x11_focused(ctx, data, request, window, display, secondary,
         managed);
   CHECK(ctx->has_focus(data));
   ctx->windows->destroy(data, window);
   CHECK(!ctx->has_focus(data));
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = native_x11_window(window);

   /* The window manager's close button */
   if (managed)
   {
      native_x11_wm_close(display, secondary);
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(native_window_closed_user);
      CHECK(native_x11_primary_intact(display, primary));
      ctx->windows->destroy(data, window);
      request->generation = video_driver_window_generation();
      CHECK(ctx->windows->create(data, request, window));
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
      secondary = native_x11_window(window);
   }

   /* Destroyed by another client while requests for it are on their
    * way: the errors close the window, not the process */
   {
      Display *intruder = XOpenDisplay(DisplayString(display));
      CHECK(intruder != NULL);
      if (intruder)
      {
         XDestroyWindow(intruder, secondary);
         XSync(intruder, False);
         XCloseDisplay(intruder);
      }
   }
   request->settings.pos = VIDEO_POS_PACK(monitors[0].x + 80,
         monitors[0].y + 80);
   ctx->windows->update(data, window, request);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(!native_window_closed_user);
   CHECK(!window->presentable);
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));
   ctx->windows->destroy(data, window);
   XSync(display, False);
   native_window_settle(ctx, data, request, 50000);
   CHECK(native_x11_primary_intact(display, primary));

   /* Game Screen 1's window on a spare id: titled for its screen */
   {
      video_window_request_t spare;
      video_window_native_t one;
      native_window_spare(request, &spare);
      memset(&one, 0, sizeof(one));
      CHECK(ctx->windows->create(data, &spare, &one));
      CHECK(native_x11_titled(display, 0) == 1);
      CHECK(native_x11_titled(display, spare.screen) == 0);
      if (one.state)
         native_x11_titles(display, primary, native_x11_window(&one), 0);
      ctx->windows->destroy(data, &one);
      XSync(display, False);
      CHECK(!native_window_settle(ctx, data, &spare, 50000));
   }

   /* Refused requests leave nothing behind and nothing retries them */
   request->generation = video_driver_window_generation();
   memset(&invalid, 0, sizeof(invalid));
   other = *request;
   other.generation = 0;
   CHECK(!ctx->windows->create(data, &other, &invalid));
   other = *request;
   other.screen = 0;
   CHECK(!ctx->windows->create(data, &other, &invalid));
   other = *request;
   other.screen = RETRO_VIDEO_VIEWS_MAX;
   CHECK(!ctx->windows->create(data, &other, &invalid));
   other = *request;
   other.settings.mode = VIDEO_WINDOW_ATTACHED;
   CHECK(!ctx->windows->create(data, &other, &invalid));
   other = *request;
   other.settings.dims = VIDEO_SCALE_PACK(0, 240);
   CHECK(!ctx->windows->create(data, &other, &invalid));
   CHECK(!invalid.state && !invalid.surface && !invalid.display);
   CHECK(!native_window_settle(ctx, data, request, 200000));
   CHECK(native_x11_titled(display, video_window_title_screen(request)) == 0);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_x11_titled(display, video_window_title_screen(request)) == 1);

   /* An error the handler takes for a window replaced before the next
    * pump does not close the replacement; one for the window does */
   handler = XSetErrorHandler(native_x11_ignore_error);
   XSetErrorHandler(handler);
   CHECK(handler != native_x11_ignore_error);
   native_x11_fake_error(handler, display, native_x11_window(window),
         BadWindow);
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 50000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->presentable);
   native_x11_fake_error(handler, display, native_x11_window(window),
         BadWindow);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(!native_window_closed_user);
   CHECK(!window->presentable);
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_x11_primary_intact(display, primary));
   native_x11_saves(ctx, data, request, window, monitors[0], managed);
   CHECK(native_x11_primary_intact(display, primary));
   native_x11_updates(ctx, data, request, window,
         monitors[g_x11_screen < count ? g_x11_screen : 0],
         (g_x11_screen < count ? g_x11_screen : 0) + 1, managed);
   CHECK(native_x11_primary_intact(display, primary));
#if defined(RARCH_HAVE_X11_MONITORS) && defined(HAVE_XRANDR) \
   && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
   native_x11_gone(ctx, data, request, window);
   CHECK(native_x11_primary_intact(display, primary));
#endif
#ifdef NATIVE_EGL
   if (strcmp(ctx->ident, "egl_x") == 0)
   {
      native_egl_failures(ctx, data, request, window, native_x11_drawable);
      CHECK(native_x11_primary_intact(display, primary));
   }
#endif
#ifdef NATIVE_GLX
   if (strcmp(ctx->ident, "x") == 0)
   {
      native_glx_failure(ctx, data, request, window);
      CHECK(native_x11_primary_intact(display, primary));
   }
#endif
}
#endif

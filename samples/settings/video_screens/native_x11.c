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
      rect = native_x11_rect((Display*)window->display,
            (Window)(uintptr_t)window->surface);
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
      rect = native_x11_rect((Display*)window->display,
            (Window)(uintptr_t)window->surface);
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
   while (native_x11_fullscreen_state((Display*)window->display,
            (Window)(uintptr_t)window->surface) != want)
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

/* An error in 'window' handed to 'handler' as Xlib would hand it */
static void native_x11_fake_error(int (*handler)(Display*, XErrorEvent*),
      Display *display, Window window)
{
   XErrorEvent event;
   memset(&event, 0, sizeof(event));
   event.display      = display;
   event.resourceid   = window;
   event.serial       = XNextRequest(display) + 100000;
   event.error_code   = BadWindow;
   event.request_code = 12; /* X_ConfigureWindow */
   CHECK(handler(display, &event) == 0);
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
   Display *dpy              = (Display*)window->display;
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
            rect = native_x11_rect((Display*)window->display,
                  (Window)(uintptr_t)window->surface);
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
#endif

/* The monitors as the screens' Display setting lists them, republished
 * when one changes or the main window goes to another, and only then */
static void native_x11_monitors_check(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request, Display *display,
      const native_rect_t *monitors)
{
#ifdef RARCH_HAVE_X11_MONITORS
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned i, n, restore, serial;
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
   serial  = x11_monitors_serial();
   x11_monitors_window(0);
   native_x11_serial_wait(serial);
   native_window_settle(ctx, data, request, 50000);
   serial  = x11_monitors_serial();
   x11_monitors_window(g_x11_win);
   native_x11_serial_wait(serial);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(video_st->windows_restore == restore);

   /* The main window's monitor follows it */
   if (n >= 2 && from >= 0)
   {
      native_rect_t was = native_x11_rect(display, g_x11_win);
      int to            = from ? 0 : 1;
      XMoveWindow(display, g_x11_win, monitors[to].x + 100,
            monitors[to].y + 100);
      XFlush(display);
      native_x11_monitors_wait(ctx, data, request, monitors, want, to, got);
      CHECK(video_st->windows_restore != restore);
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
   Display *display = (Display*)window->display;
   Window primary   = g_x11_win;
   Window secondary = (Window)(uintptr_t)window->surface;
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
   CHECK(native_x11_titled(display, request->screen) == 1);
   native_x11_titles(display, primary, secondary, request->screen);
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
   secondary = (Window)(uintptr_t)window->surface;

   /* Closing the focused window takes its focus with it */
   native_x11_focus(display, secondary, managed);
   {
      retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
      while (!window->focused && cpu_features_get_time_usec() < deadline)
      {
         native_window_drain(ctx, data, request);
         retro_sleep(2);
      }
   }
   native_window_settle(ctx, data, request, 100000);
   CHECK(window->focused);
   native_x11_close(display, secondary);
   XSync(display, False);
   seen = native_window_drain(ctx, data, request);
   CHECK(seen & NATIVE_EVENT(CLOSE));
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->focused && !window->presentable);
   ctx->windows->destroy(data, window);
   /* Xlib's default handler back, as a save and restore elsewhere can
    * leave it: the next create has to put the window handler back */
   XSetErrorHandler(NULL);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = (Window)(uintptr_t)window->surface;

   /* The window manager's close button */
   if (managed)
   {
      native_x11_wm_close(display, secondary);
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(native_x11_primary_intact(display, primary));
      ctx->windows->destroy(data, window);
      request->generation = video_driver_window_generation();
      CHECK(ctx->windows->create(data, request, window));
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
      secondary = (Window)(uintptr_t)window->surface;
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
   CHECK(!window->presentable);
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));
   ctx->windows->destroy(data, window);
   XSync(display, False);
   native_window_settle(ctx, data, request, 50000);
   CHECK(native_x11_primary_intact(display, primary));

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
   CHECK(native_x11_titled(display, request->screen) == 0);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_x11_titled(display, request->screen) == 1);

   /* An error the handler takes for a window replaced before the next
    * pump does not close the replacement; one for the window does */
   handler = XSetErrorHandler(native_x11_ignore_error);
   XSetErrorHandler(handler);
   CHECK(handler != native_x11_ignore_error);
   native_x11_fake_error(handler, display, (Window)(uintptr_t)window->surface);
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 50000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->presentable);
   native_x11_fake_error(handler, display, (Window)(uintptr_t)window->surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(!window->presentable);
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_x11_primary_intact(display, primary));
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

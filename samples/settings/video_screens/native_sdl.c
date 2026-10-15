/* The SDL contexts' native-window lifecycle for video_screens_test.c,
 * #included there. A build has SDL 2 or SDL 3, never both. */
#ifdef NATIVE_SDL
#ifdef HAVE_SDL3
#define NATIVE_SDL_KIND    VIDEO_WINDOW_NATIVE_SDL3
#define NATIVE_SDL_CLOSE   SDL_EVENT_WINDOW_CLOSE_REQUESTED
#define NATIVE_SDL_UNFOCUS SDL_EVENT_WINDOW_FOCUS_LOST
#define NATIVE_SDL_RESIZE  SDL_EVENT_WINDOW_RESIZED
typedef sdl3_video_window_t native_sdl_state_t;
#else
#define NATIVE_SDL_KIND    VIDEO_WINDOW_NATIVE_SDL2
#define NATIVE_SDL_CLOSE   SDL_WINDOWEVENT_CLOSE
#define NATIVE_SDL_UNFOCUS SDL_WINDOWEVENT_FOCUS_LOST
#define NATIVE_SDL_RESIZE  SDL_WINDOWEVENT_RESIZED
typedef sdl2_video_window_t native_sdl_state_t;
#endif
#define NATIVE_SDL_FLAGS (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN \
      | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_OPENGL)

static bool native_sdl_ident(const char *ident)
{
#ifdef HAVE_SDL3
   return    strcmp(ident, "gl_sdl3") == 0
          || strcmp(ident, "vk_sdl3") == 0;
#else
   return strcmp(ident, "gl_sdl") == 0;
#endif
}

/* A context's data starts with its main window */
static SDL_Window *native_sdl_main(void *data)
{
   return *(SDL_Window**)data;
}

static bool native_sdl_driver(const char *name)
{
   const char *driver = SDL_GetCurrentVideoDriver();
   return driver && strcmp(driver, name) == 0;
}

static float native_sdl_density(SDL_Window *win)
{
#ifdef HAVE_SDL3
   float density = SDL_GetWindowPixelDensity(win);
   if (density > 0.0f)
      return density;
#endif
   return 1.0f;
}

static int native_sdl_interval(void)
{
#ifdef HAVE_SDL3
   int interval = -100;
   SDL_GL_GetSwapInterval(&interval);
   return interval;
#else
   return SDL_GL_GetSwapInterval();
#endif
}

/* SDL's displays in its order: monitor N is entry N - 1 */
static unsigned native_sdl_displays(SDL_Rect *out, unsigned capacity)
{
   unsigned n = 0;
#ifdef HAVE_SDL3
   int i, count     = 0;
   SDL_DisplayID *ids = SDL_GetDisplays(&count);
   for (i = 0; ids && i < count && n < capacity; i++)
      if (SDL_GetDisplayBounds(ids[i], &out[n]))
         n++;
   SDL_free(ids);
#else
   int i, count = SDL_GetNumVideoDisplays();
   for (i = 0; i < count && n < capacity; i++)
      if (SDL_GetDisplayBounds(i, &out[n]) == 0)
         n++;
#endif
   return n;
}

/* The entry of the display SDL has the window on */
static unsigned native_sdl_display_of(SDL_Window *win)
{
#ifdef HAVE_SDL3
   int i, count       = 0;
   unsigned found     = 0;
   SDL_DisplayID id   = SDL_GetDisplayForWindow(win);
   SDL_DisplayID *ids = SDL_GetDisplays(&count);
   for (i = 0; ids && i < count; i++)
      if (ids[i] == id)
         found = (unsigned)i;
   SDL_free(ids);
   return found;
#else
   int i = SDL_GetWindowDisplayIndex(win);
   return i > 0 ? (unsigned)i : 0;
#endif
}

/* The refresh rate of the display SDL has the window on, 0 if unknown */
static float native_sdl_refresh(SDL_Window *win)
{
#ifdef HAVE_SDL3
   const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(
         SDL_GetDisplayForWindow(win));
   return mode ? mode->refresh_rate : 0.0f;
#else
   SDL_DisplayMode mode;
   if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(win), &mode))
      return 0.0f;
   return (float)mode.refresh_rate;
#endif
}

#ifdef HAVE_X11
static bool native_sdl_x11(SDL_Window *win, Display **dpy, Window *xwin)
{
#ifdef HAVE_SDL3
   SDL_PropertiesID props = SDL_GetWindowProperties(win);
   *dpy  = (Display*)SDL_GetPointerProperty(props,
         SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
   *xwin = (Window)SDL_GetNumberProperty(props,
         SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#else
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);
   *dpy  = NULL;
   *xwin = 0;
   if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_X11)
   {
      *dpy  = info.info.x11.display;
      *xwin = info.info.x11.window;
   }
#endif
   return *dpy && *xwin;
}
#endif

#ifdef NATIVE_WIN32
static HWND native_sdl_hwnd(SDL_Window *win)
{
#ifdef HAVE_SDL3
   return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(win),
         SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#else
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);
   if (     SDL_GetWindowWMInfo(win, &info)
         && info.subsystem == SDL_SYSWM_WINDOWS)
      return info.info.win.window;
   return NULL;
#endif
}
#endif

/* The nested KWin the harness may run this under, which can also act
 * on a Wayland window */
static bool native_sdl_kwin(unsigned screen, const char *body)
{
#ifdef HAVE_WAYLAND
   return native_wl_kwin(screen, body);
#else
   return false;
#endif
}

/* Where the window's drawable area is, in SDL's window coordinates: as
 * the window system has it where it tells, else as SDL has it */
static SDL_Rect native_sdl_rect(SDL_Window *win)
{
   SDL_Rect rect;
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   if (native_sdl_x11(win, &dpy, &xwin))
   {
      native_rect_t x = native_x11_rect(dpy, xwin);
      rect.x = x.x;
      rect.y = x.y;
      rect.w = (int)x.w;
      rect.h = (int)x.h;
      return rect;
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(win))
   {
      RECT client = native_w32_client(native_sdl_hwnd(win));
      rect.x = client.left;
      rect.y = client.top;
      rect.w = client.right  - client.left;
      rect.h = client.bottom - client.top;
      return rect;
   }
#endif
   SDL_GetWindowPosition(win, &rect.x, &rect.y);
   SDL_GetWindowSize(win, &rect.w, &rect.h);
   return rect;
}

/* Where saved bounds x,y w*h belong on 'monitor': moved onto it where
 * they are partly on it as saved, else centred on it, as when automatic;
 * no bigger than it */
static SDL_Rect native_sdl_expect(const SDL_Rect *monitor, int x, int y,
      int w, int h, bool automatic)
{
   SDL_Rect rect;
   rect.w = MIN(w, monitor->w);
   rect.h = MIN(h, monitor->h);
   rect.x = x;
   rect.y = y;
   if (     automatic
         || x >= monitor->x + monitor->w || x + w <= monitor->x
         || y >= monitor->y + monitor->h || y + h <= monitor->y)
   {
      rect.x = monitor->x + (monitor->w - rect.w) / 2;
      rect.y = monitor->y + (monitor->h - rect.h) / 2;
      return rect;
   }
   if (rect.x < monitor->x)
      rect.x = monitor->x;
   if (rect.y < monitor->y)
      rect.y = monitor->y;
   if (rect.x + rect.w > monitor->x + monitor->w)
      rect.x = monitor->x + monitor->w - rect.w;
   if (rect.y + rect.h > monitor->y + monitor->h)
      rect.y = monitor->y + monitor->h - rect.h;
   return rect;
}

/* Pumps until the window's drawable area is 'want' (anywhere with an
 * empty one), and the record and, with 'event', the last geometry event
 * say so too. Where the compositor places windows, its size only. */
static void native_sdl_wait_rect_at(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      const video_window_native_t *window, SDL_Rect want, bool event,
      int slack)
{
   SDL_Rect rect;
   SDL_Window *win       = (SDL_Window*)window->surface;
   bool anywhere         = !want.w;
   bool placed           = !native_sdl_driver("wayland");
   float density         = native_sdl_density(win);
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      rect = native_sdl_rect(win);
      if (anywhere)
         want = rect;
      if (     rect.w == want.w && rect.h == want.h
            && window->dims == VIDEO_SCALE_PACK(
               (unsigned)(want.w * density), (unsigned)(want.h * density))
            && (!placed || (abs(rect.x - want.x) <= slack
                  && abs(rect.y - want.y) <= slack
                  && (!event || native_window_pos
                     == VIDEO_POS_PACK(rect.x, rect.y)))))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: at %d,%d %dx%d (event %d,%d, record "
         "%ux%u), wanted %d,%d %dx%d\n", rect.x, rect.y, rect.w, rect.h,
         VIDEO_POS_X(native_window_pos), VIDEO_POS_Y(native_window_pos),
         VIDEO_SCALE_W(window->dims), VIDEO_SCALE_H(window->dims),
         want.x, want.y, want.w, want.h);
   CHECK(!"window bounds");
}

static void native_sdl_wait_rect(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, SDL_Rect want)
{
   native_sdl_wait_rect_at(ctx, data, request, window, want, true, 0);
}

/* Pumps until the window is wholly on 'monitor' */
static void native_sdl_wait_inside(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, const SDL_Rect *monitor)
{
   SDL_Rect rect;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      rect = native_sdl_rect((SDL_Window*)window->surface);
      if (     rect.x >= monitor->x && rect.y >= monitor->y
            && rect.x + rect.w <= monitor->x + monitor->w
            && rect.y + rect.h <= monitor->y + monitor->h
            && native_window_pos == VIDEO_POS_PACK(rect.x, rect.y))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   CHECK(!"window monitor");
}

/* Pumps until the window is fullscreen (or not) on display 'on'; with
 * 'made', it was made so, at the position it has */
static void native_sdl_wait_fullscreen(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      const video_window_native_t *window, const SDL_Rect *monitors,
      unsigned on, bool want, bool made)
{
   SDL_Window *win = (SDL_Window*)window->surface;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      bool now;
      native_window_drain(ctx, data, request);
      now = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
      if (now == want && (!want || native_sdl_display_of(win) == on))
         break;
      if (cpu_features_get_time_usec() > deadline)
      {
         fprintf(stderr, "native window: fullscreen %d on %u, wanted %d "
               "on %u\n", now, native_sdl_display_of(win), want, on);
         CHECK(!"fullscreen state");
         return;
      }
      retro_sleep(2);
   }
   if (want)
      native_sdl_wait_rect_at(ctx, data, request, window, monitors[on],
            !made, 0);
}

/* The main window's state, which a secondary must leave alone */
typedef struct
{
   SDL_Window *win;
   uintptr_t display;
   uintptr_t handle;
   SDL_Rect rect;
   unsigned flags;
   unsigned id;
} native_sdl_main_t;

static void native_sdl_main_get(void *data, native_sdl_main_t *out)
{
   memset(out, 0, sizeof(*out));
   out->win     = native_sdl_main(data);
   out->display = video_driver_display_get();
   out->handle  = video_driver_window_get();
   out->id      = (unsigned)SDL_GetWindowID(out->win);
   out->flags   = (unsigned)(SDL_GetWindowFlags(out->win) & NATIVE_SDL_FLAGS);
   /* where the compositor places windows, SDL makes one up */
   if (!native_sdl_driver("wayland"))
      SDL_GetWindowPosition(out->win, &out->rect.x, &out->rect.y);
   SDL_GetWindowSize(out->win, &out->rect.w, &out->rect.h);
}

static bool native_sdl_main_intact(void *data, const native_sdl_main_t *was)
{
   native_sdl_main_t now;
   native_sdl_main_get(data, &now);
   if (memcmp(&now, was, sizeof(now)))
      fprintf(stderr, "native main: %d,%d %dx%d flags 0x%x, was %d,%d "
            "%dx%d flags 0x%x\n", now.rect.x, now.rect.y, now.rect.w,
            now.rect.h, now.flags, was->rect.x, was->rect.y, was->rect.w,
            was->rect.h, was->flags);
   return    now.win && now.win == was->win
          && SDL_GetWindowFromID(was->id) == was->win
          && video_driver_display_userdata_get() == (uintptr_t)now.win
          && now.display == was->display
          && now.handle  == was->handle
          && now.flags   == was->flags
          && now.rect.x  == was->rect.x && now.rect.y == was->rect.y
          && now.rect.w  == was->rect.w && now.rect.h == was->rect.h;
}

/* Windows named for 'screen' */
static unsigned native_sdl_titled(unsigned screen)
{
   char want[64];
   unsigned found = 0;
#ifdef HAVE_SDL3
   int i, count       = 0;
   SDL_Window **list  = SDL_GetWindows(&count);
#else
   Uint32 i;
#endif
   snprintf(want, sizeof(want), "RetroArch - Screen %u", screen + 1);
#ifdef HAVE_SDL3
   for (i = 0; list && i < count; i++)
      found += strcmp(SDL_GetWindowTitle(list[i]), want) == 0;
   SDL_free(list);
#else
   /* window IDs count up from 1 */
   for (i = 1; i < 4096; i++)
   {
      SDL_Window *win = SDL_GetWindowFromID(i);
      found += win && strcmp(SDL_GetWindowTitle(win), want) == 0;
   }
#endif
   return found;
}

/* A window event of SDL's, as if the window system had sent it */
static void native_sdl_push(SDL_Window *win, unsigned kind, int a, int b)
{
   SDL_Event event;
   memset(&event, 0, sizeof(event));
#ifdef HAVE_SDL3
   event.type         = kind;
#else
   event.type         = SDL_WINDOWEVENT;
   event.window.event = (Uint8)kind;
#endif
   event.window.windowID = SDL_GetWindowID(win);
   event.window.data1    = a;
   event.window.data2    = b;
   CHECK(SDL_PushEvent(&event) > 0);
}

/* Moved or resized as a user would, from outside SDL, to drawable-area
 * bounds: through the window manager, or by SDL where windows are the
 * compositor's to place */
static void native_sdl_user(SDL_Window *win, int x, int y, int w, int h)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   if (native_sdl_x11(win, &dpy, &xwin))
   {
      XEvent event;
      memset(&event, 0, sizeof(event));
      event.xclient.type         = ClientMessage;
      event.xclient.window       = xwin;
      event.xclient.message_type = XInternAtom(dpy,
            "_NET_MOVERESIZE_WINDOW", False);
      event.xclient.format       = 32;
      /* StaticGravity, from a pager: the window's own bounds */
      event.xclient.data.l[0]    = StaticGravity | (2 << 12)
         | (w ? (3 << 10) : (3 << 8));
      event.xclient.data.l[1]    = x;
      event.xclient.data.l[2]    = y;
      event.xclient.data.l[3]    = w;
      event.xclient.data.l[4]    = h;
      XSendEvent(dpy, DefaultRootWindow(dpy), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &event);
      XFlush(dpy);
      return;
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(win))
   {
      HWND hwnd = native_sdl_hwnd(win);
      RECT rect = native_w32_client(hwnd);
      if (w)
      {
         rect.right  = rect.left + w;
         rect.bottom = rect.top  + h;
      }
      else
         OffsetRect(&rect, x - rect.left, y - rect.top);
      AdjustWindowRectEx(&rect, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
            FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
      SetWindowPos(hwnd, NULL, rect.left, rect.top, rect.right - rect.left,
            rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
      return;
   }
#endif
   if (w)
      SDL_SetWindowSize(win, w, h);
   else
      SDL_SetWindowPosition(win, x, y);
}

/* Closed as its close button would: through the window system */
static bool native_sdl_close(SDL_Window *win, unsigned screen, bool main)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   if (native_sdl_x11(win, &dpy, &xwin))
   {
      native_x11_close(dpy, xwin);
      return true;
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(win))
      return PostMessage(native_sdl_hwnd(win), WM_CLOSE, 0, 0) != 0;
#endif
   return native_sdl_kwin(screen, main ? "if (m) m.closeWindow();"
         : "w.closeWindow();");
}

/* Gives the keyboard focus to the window, through the window manager;
 * false where no window can have it (a desktop that is never shown,
 * where SDL, going by the foreground window, reports no focus at all) */
static bool native_sdl_activate(SDL_Window *win, unsigned screen, bool main)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   if (native_sdl_x11(win, &dpy, &xwin))
   {
      native_x11_focus(dpy, xwin, true);
      return true;
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(win))
   {
      SetForegroundWindow(native_sdl_hwnd(win));
      SetFocus(native_sdl_hwnd(win));
      return GetForegroundWindow() == native_sdl_hwnd(win);
   }
#endif
   return native_sdl_kwin(screen, main ? "if (m) workspace.activeWindow = m;"
         : "workspace.activeWindow = w;");
}

/* Focus given to the window, from the main window, or back to that;
 * false where there is none to give */
static bool native_sdl_focus(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, bool on)
{
   unsigned seen         = 0;
   SDL_Window *primary   = native_sdl_main(data);
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   /* a window manager may have given it the focus already */
   if (on && window->focused)
   {
      CHECK(native_sdl_activate(primary, request->screen, true));
      while (window->focused && cpu_features_get_time_usec() < deadline)
      {
         seen |= native_window_drain(ctx, data, request);
         retro_sleep(2);
      }
   }
   if (!native_sdl_activate(on ? (SDL_Window*)window->surface : primary,
            request->screen, !on))
   {
      fprintf(stderr, "native sdl: no focus to give here\n");
      return false;
   }
   seen |= native_window_wait(ctx, data, request, window,
         on ? NATIVE_EVENT(FOCUS_GAIN) : NATIVE_EVENT(FOCUS_LOSS), 0);
   CHECK(window->focused == on);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   return true;
}

/* Back from minimised: a Wayland client cannot ask for it */
static bool native_sdl_unminimise(SDL_Window *win, unsigned screen)
{
   if (!native_sdl_driver("wayland"))
   {
      SDL_RestoreWindow(win);
      return true;
   }
   return native_sdl_kwin(screen,
         "w.minimized = false; workspace.activeWindow = w;");
}

/* Whether the main window's interval reads 1 as set, sampled before
 * any secondary is bound: not where the GL stack has no swap control */
static bool native_sdl_intervals;

/* A Wayland compositor maps a window once something is on it: a frame
 * where the context draws. Without one (Vulkan) the compositor's own
 * steps are skipped. */
static bool native_sdl_map(const gfx_ctx_driver_t *ctx, void *data,
      video_window_native_t *window)
{
   if (!native_sdl_driver("wayland"))
      return true;
   if (!ctx->windows->bind_gl)
      return false;
   CHECK(ctx->windows->bind_gl(data, window));
   CHECK(ctx->windows->present_gl(data, window));
   CHECK(ctx->windows->bind_gl(data, NULL));
   CHECK(!native_sdl_intervals || native_sdl_interval() == 1);
   return true;
}

static void native_sdl_replace(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL && window->surface == NULL);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   native_sdl_map(ctx, data, window);
}

/* Pumps until the main window's close quits */
static bool native_sdl_wait_quit(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, unsigned *seen)
{
   video_window_event_t events[4];
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   do
   {
      unsigned i, count;
      unsigned dims = VIDEO_SCALE_PACK(640, 480);
      bool quit     = false;
      bool resize   = false;
      ctx->check_window(data, &quit, &resize, &dims);
      while ((count = ctx->windows->poll(data, events, 4)))
         for (i = 0; i < count; i++)
         {
            CHECK(events[i].generation == request->generation);
            *seen |= 1u << events[i].kind;
         }
      if (quit)
         return true;
      retro_sleep(2);
   } while (cpu_features_get_time_usec() < deadline);
   return false;
}

/* A resize of the main window, as SDL reports one, still reaches the
 * context's own handling, which reports the main window's size */
static void native_sdl_main_resize(const gfx_ctx_driver_t *ctx, void *data)
{
   int w, h;
   unsigned dims       = 0;
   bool quit           = false;
   bool resize         = false;
   SDL_Window *primary = native_sdl_main(data);
#ifdef HAVE_SDL3
   SDL_GetWindowSizeInPixels(primary, &w, &h);
#else
   SDL_GetWindowSize(primary, &w, &h);
#endif
   native_sdl_push(primary, NATIVE_SDL_RESIZE, w, h);
   ctx->check_window(data, &quit, &resize, &dims);
   CHECK(!quit && resize);
   CHECK(dims == VIDEO_SCALE_PACK(w, h));
}

/* Pumps SDL (not the context) until the main window's close request is
 * queued */
static bool native_sdl_close_queued(SDL_Window *primary)
{
   static SDL_Event events[32];
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   do
   {
      int i, count;
      SDL_PumpEvents();
#ifdef HAVE_SDL3
      count = SDL_PeepEvents(events, 32, SDL_PEEKEVENT,
            SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST);
#else
      count = SDL_PeepEvents(events, 32, SDL_PEEKEVENT,
            SDL_WINDOWEVENT, SDL_WINDOWEVENT);
#endif
      for (i = 0; i < count; i++)
         if (     events[i].window.windowID == SDL_GetWindowID(primary)
#ifdef HAVE_SDL3
               && events[i].type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
#else
               && events[i].window.event == SDL_WINDOWEVENT_CLOSE)
#endif
            return true;
      retro_sleep(2);
   } while (cpu_features_get_time_usec() < deadline);
   return false;
}

static void native_sdl_gl(const gfx_ctx_driver_t *ctx, void *data,
      video_window_native_t *window)
{
   unsigned i, j;
   retro_time_t start, elapsed;
   retro_time_t period   = 0;
   retro_time_t limit    = 0;
   SDL_Window *primary   = native_sdl_main(data);
   SDL_GLContext context = SDL_GL_GetCurrentContext();
   float refresh         = native_sdl_refresh((SDL_Window*)window->surface);
   if (!ctx->windows->bind_gl)
   {
      CHECK(ctx->windows->present_gl == NULL);
      return;
   }
   CHECK(SDL_GL_GetCurrentWindow() == primary && context != NULL);
#if defined(HAVE_SDL3) && defined(NATIVE_WIN32)
   /* A desktop that is never shown (an SSH session's) fails every swap
    * after the first, the main window's too */
   if (!SDL_GL_SwapWindow(primary) || !SDL_GL_SwapWindow(primary))
   {
      fprintf(stderr, "native sdl: swaps fail on this desktop, presents "
            "not checked\n");
      return;
   }
#endif
#ifdef NATIVE_EGL
   fprintf(stderr, "native sdl: GL through %s\n",
         eglGetCurrentContext() != EGL_NO_CONTEXT ? "EGL" : "the platform");
#endif
   /* Presents are timed where 20 at interval 1 take at least 0.6 of 20
    * frame periods: 30 at interval 0 must take under 0.6 of 30 */
   if (refresh > 0.0f)
   {
      period = (retro_time_t)(1000000.0 / refresh);
      CHECK(ctx->windows->bind_gl(data, window));
      SDL_GL_SetSwapInterval(1);
      start = cpu_features_get_time_usec();
      for (j = 0; j < 20
            && cpu_features_get_time_usec() - start < 12 * period; j++)
         ctx->windows->present_gl(data, window);
      elapsed = cpu_features_get_time_usec() - start;
      if (elapsed >= 12 * period)
         limit = 18 * period;
      CHECK(ctx->windows->bind_gl(data, NULL));
      fprintf(stderr, "native sdl: %.2f Hz, %u presents at interval 1 in "
            "%ld us, %s\n", refresh, j, (long)elapsed,
            limit ? "presents timed" : "no vsync, presents not timed");
   }
   else
      fprintf(stderr, "native sdl: refresh unknown, presents not timed\n");
   /* Twice with the main window's vsync off, then twice with it on: the
    * secondary swaps at 0 either way */
   for (i = 0; i < 4; i++)
   {
      int interval = i < 2 ? 0 : 1;
      if (i == 0 || i == 2)
         ctx->swap_interval(data, interval);
      CHECK(ctx->windows->bind_gl(data, window));
      CHECK(SDL_GL_GetCurrentWindow() == (SDL_Window*)window->surface);
      CHECK(SDL_GL_GetCurrentContext() == context);
      CHECK(!native_sdl_intervals || native_sdl_interval() == 0);
      start = cpu_features_get_time_usec();
      for (j = 0; j < 30; j++)
         CHECK(ctx->windows->present_gl(data, window));
      elapsed = cpu_features_get_time_usec() - start;
      if (limit)
         fprintf(stderr, "native sdl: 30 presents in %ld us, limit %ld "
               "us\n", (long)elapsed, (long)limit);
      else
         fprintf(stderr, "native sdl: 30 presents in %ld us\n",
               (long)elapsed);
      CHECK(!limit || elapsed < limit);
      CHECK(ctx->windows->bind_gl(data, NULL));
      CHECK(SDL_GL_GetCurrentWindow() == primary);
      CHECK(SDL_GL_GetCurrentContext() == context);
      CHECK(!native_sdl_intervals || native_sdl_interval() == interval);
   }
}

/* A secondary that fails is closed, and then replaced: a present with
 * another window current cannot swap, and a bind of a window that is
 * not an OpenGL one fails, the main window's interval back after it */
static void native_sdl_gl_failures(const gfx_ctx_driver_t *ctx,
      void *data, video_window_request_t *request,
      video_window_native_t *window)
{
   unsigned i;
   SDL_Window *primary = native_sdl_main(data);
#ifdef HAVE_SDL3
   SDL_Window *plain   = SDL_CreateWindow("plain", 16, 16, SDL_WINDOW_HIDDEN);
#else
   SDL_Window *plain   = SDL_CreateWindow("plain", 0, 0, 16, 16,
         SDL_WINDOW_HIDDEN);
#endif
   CHECK(plain != NULL);
   for (i = 0; plain && i < 2; i++)
   {
      native_sdl_state_t *state = (native_sdl_state_t*)window->state;
      SDL_Window *good          = state->win;
      retro_time_t deadline     = cpu_features_get_time_usec() + 3000000;
      /* a present to a window that is not presentable is a skip */
      while (     !window->presentable
               && cpu_features_get_time_usec() < deadline)
      {
         native_window_drain(ctx, data, request);
         retro_sleep(2);
      }
      if (!window->presentable)
      {
         fprintf(stderr, "native sdl: not presentable, GL failures "
               "not forced\n");
         CHECK(window->presentable);
         break;
      }
      if (!i)
      {
         CHECK(ctx->windows->bind_gl(data, NULL));
         CHECK(!ctx->windows->present_gl(data, window));
      }
      else
      {
         /* The main window's interval at the secondary's 0, as a
          * driver with one per context leaves it */
         CHECK(ctx->windows->bind_gl(data, window));
#ifdef HAVE_SDL3
         CHECK(SDL_GL_MakeCurrent(primary, SDL_GL_GetCurrentContext()));
#else
         CHECK(SDL_GL_MakeCurrent(primary, SDL_GL_GetCurrentContext()) == 0);
#endif
         SDL_GL_SetSwapInterval(0);
         state->win = plain;
         CHECK(!ctx->windows->bind_gl(data, window));
         state->win = good;
         CHECK(!native_sdl_intervals || native_sdl_interval() == 1);
      }
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(!window->presentable);
      CHECK(!ctx->windows->bind_gl(data, window));
      CHECK(SDL_GL_GetCurrentWindow() == primary);
      native_sdl_replace(ctx, data, request, window);
   }
   if (plain)
      SDL_DestroyWindow(plain);
}

static void native_sdl_lifecycle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   SDL_Rect monitors[8];
   SDL_Rect home, rect, before;
   native_sdl_main_t main_state;
   video_window_request_t other;
   video_window_native_t invalid;
   unsigned i, seen, last, on;
   float density;
   SDL_Window *primary   = native_sdl_main(data);
   SDL_Window *secondary = (SDL_Window*)window->surface;
   unsigned count        = native_sdl_displays(monitors, 8);
   bool placed           = !native_sdl_driver("wayland");
   bool mapped;

#ifdef HAVE_SDL3
   fprintf(stderr, "native sdl: SDL %d.%d.%d, %s, %u display(s)\n",
         SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
         SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
         SDL_VERSIONNUM_MICRO(SDL_GetVersion()),
         SDL_GetCurrentVideoDriver(), count);
#else
   {
      SDL_version linked;
      SDL_GetVersion(&linked);
      fprintf(stderr, "native sdl: SDL %d.%d.%d, %s, %u display(s)\n",
            linked.major, linked.minor, linked.patch,
            SDL_GetCurrentVideoDriver(), count);
   }
#endif
   CHECK(count > 0);
   if (!count)
      return;
   native_sdl_intervals = false;
   if (ctx->windows->bind_gl)
   {
      int interval         = native_sdl_interval();
      native_sdl_intervals = interval == 1;
      if (!native_sdl_intervals)
         fprintf(stderr, "native sdl: main interval reads %d, intervals "
               "not checked\n", interval);
      /* A Wayland compositor shows the main window once it has a frame */
      if (!placed)
         ctx->swap_buffers(data);
   }
   home = monitors[native_sdl_display_of(primary)];
   CHECK(window->display == NULL);
   CHECK(secondary && secondary != primary);
   CHECK(SDL_GetWindowFlags(secondary) & (ctx->windows->bind_gl
         ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN));
   CHECK(native_sdl_titled(request->screen) == 1);
   native_window_titles(SDL_GetWindowTitle(secondary),
         SDL_GetWindowTitle(primary), request->screen);
#ifdef HAVE_X11
   {
      Display *dpy, *main_dpy;
      Window xwin, main_xwin;
      if (     native_sdl_x11(secondary, &dpy, &xwin)
            && native_sdl_x11(primary, &main_dpy, &main_xwin))
         native_x11_titles(dpy, main_xwin, xwin, request->screen);
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(secondary) && native_sdl_hwnd(primary))
      native_w32_titles(native_sdl_hwnd(primary), native_sdl_hwnd(secondary),
            request->screen);
#endif
   /* After a restart, what was queued for the last lifetime's window
    * (a close and a resize) has to reach nothing */
   seen  = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   if (!(mapped = native_sdl_map(ctx, data, window)))
      fprintf(stderr, "native sdl: nothing on the window, so no "
            "compositor steps\n");
   seen |= native_window_settle(ctx, data, request, 100000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->dims == request->settings.dims);
   CHECK(window->configured && window->presentable);
   /* Shown without taking the focus from the main window, where the
    * window system leaves that to the client */
   if (native_sdl_driver("windows"))
      CHECK(!window->focused);
   /* as the window manager has settled the main window */
   native_sdl_main_get(data, &main_state);
   density = native_sdl_density(secondary);

   /* Resized by the user */
   native_sdl_user(secondary, 0, 0, (int)(400 / density),
         (int)(300 / density));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
         VIDEO_SCALE_PACK(400, 300));
   memset(&rect, 0, sizeof(rect));
   native_sdl_wait_rect(ctx, data, request, window, rect);

   if (placed)
   {
      /* A preferred monitor, with saved bounds that are not on it:
       * centred where they do not reach it, moved onto it where they do */
      request->settings.monitor = count;
      request->settings.pos     = VIDEO_POS_PACK(-200, -200);
      ctx->windows->update(data, window, request);
      if (monitors[count - 1].x >= 120 || monitors[count - 1].y >= 40)
         native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
               &monitors[count - 1], -200, -200, 320, 240, false));
      else
      {
         native_window_wait(ctx, data, request, window,
               NATIVE_EVENT(GEOMETRY), VIDEO_SCALE_PACK(320, 240));
         native_sdl_wait_inside(ctx, data, request, window,
               &monitors[count - 1]);
      }

      /* No preferred monitor: the one the bounds are mostly on, here
       * partly off its left edge */
      request->settings.monitor = 0;
      request->settings.pos     = VIDEO_POS_PACK(home.x - 100, home.y + 60);
      ctx->windows->update(data, window, request);
      native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
            &home, home.x - 100, home.y + 60, 320, 240, false));

      /* ... and saved bounds on none: the main window's */
      request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
      ctx->windows->update(data, window, request);
      native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
            &home, 0, 0, 320, 240, true));

      /* A saved monitor that is gone: the main window's instead */
      request->settings.monitor = count;
      request->settings.pos     = VIDEO_POS_PACK(monitors[count - 1].x + 10,
            monitors[count - 1].y + 40);
      ctx->windows->update(data, window, request);
      native_window_settle(ctx, data, request, 50000);
      request->settings.monitor = count + 3;
      request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
      ctx->windows->update(data, window, request);
      native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
            &home, 0, 0, 320, 240, true));
      CHECK(native_sdl_main_intact(data, &main_state));

      /* Moved by the user onto the last monitor */
      before = native_sdl_expect(&monitors[count - 1],
            monitors[count - 1].x + 40, monitors[count - 1].y + 40,
            320, 240, false);
      native_sdl_user(secondary, before.x, before.y, 0, 0);
      native_sdl_wait_rect(ctx, data, request, window, before);
   }
   else
   {
      /* Sizes are the client's own; where it goes is the compositor's */
      request->settings.dims = VIDEO_SCALE_PACK(360, 270);
      ctx->windows->update(data, window, request);
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
            VIDEO_SCALE_PACK(360, 270));
      request->settings.dims = VIDEO_SCALE_PACK(320, 240);
      ctx->windows->update(data, window, request);
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
            VIDEO_SCALE_PACK(320, 240));
      before = native_sdl_rect(secondary);
   }

   /* Borderless fullscreen where it is, then on a preferred monitor */
   on = native_sdl_display_of(secondary);
   request->settings.monitor    = 0;
   request->settings.fullscreen = true;
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         on, true, false);
   request->settings.monitor    = 1;
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         0, true, false);

   /* A window again with automatic bounds: where it stood before */
   request->settings.fullscreen = false;
   request->settings.monitor    = 0;
   request->settings.dims       = 0;
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         0, false, false);
   native_sdl_wait_rect(ctx, data, request, window, before);

   /* Fullscreen again, then a window at a saved origin */
   on = native_sdl_display_of(secondary);
   request->settings.dims       = VIDEO_SCALE_PACK(320, 240);
   request->settings.fullscreen = true;
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         on, true, false);
   request->settings.fullscreen = false;
   request->settings.pos        = VIDEO_POS_PACK(monitors[0].x + 50,
         monitors[0].y + 50);
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         0, false, false);
   native_sdl_wait_rect(ctx, data, request, window, placed
         ? native_sdl_expect(&monitors[0], monitors[0].x + 50,
            monitors[0].y + 50, 320, 240, false)
         : before);
   CHECK(native_sdl_main_intact(data, &main_state));

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

   /* Minimised and restored: skipped meanwhile, never closed. A move
    * asked for meanwhile is made once it is back. */
   if (mapped)
   {
      bool minimised;
      SDL_MinimizeWindow(secondary);
      /* SDL 2's own Wayland backend has no minimised state; SDL 3's
       * has it at once */
      minimised = !native_sdl_driver("wayland")
         || (SDL_GetWindowFlags(secondary) & SDL_WINDOW_MINIMIZED);
      if (minimised)
         seen = native_window_wait(ctx, data, request, window,
               NATIVE_EVENT(SUSPEND), 0);
      else
      {
         seen = native_window_settle(ctx, data, request, 200000);
         fprintf(stderr, "native sdl: SDL has no minimised window here\n");
      }
      CHECK(window->presentable == !minimised);
      CHECK(window->dims == VIDEO_SCALE_PACK(320, 240));
      request->settings.pos = VIDEO_POS_PACK(monitors[0].x + 70,
            monitors[0].y + 70);
      ctx->windows->update(data, window, request);
      seen |= native_window_settle(ctx, data, request, 50000);
      CHECK(window->presentable == !minimised);
      CHECK(native_sdl_unminimise(secondary, request->screen));
      if (minimised)
         seen |= native_window_wait(ctx, data, request, window,
               NATIVE_EVENT(RESUME), 0);
      else
         seen |= native_window_settle(ctx, data, request, 100000);
      CHECK(window->presentable);
      CHECK(!(seen & NATIVE_EVENT(CLOSE)));
      if (placed)
         native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
               &monitors[0], monitors[0].x + 70, monitors[0].y + 70,
               320, 240, false));
      CHECK(native_sdl_main_intact(data, &main_state));
   }

   /* Maximised by the user, then placed: from its restored state */
   if (placed)
   {
      retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
      SDL_MaximizeWindow(secondary);
      while (     !(SDL_GetWindowFlags(secondary) & SDL_WINDOW_MAXIMIZED)
               && cpu_features_get_time_usec() < deadline)
      {
         native_window_drain(ctx, data, request);
         retro_sleep(2);
      }
      CHECK(SDL_GetWindowFlags(secondary) & SDL_WINDOW_MAXIMIZED);
      request->settings.pos = VIDEO_POS_PACK(monitors[0].x + 60,
            monitors[0].y + 60);
      ctx->windows->update(data, window, request);
      /* SDL 3 may move it with the frame it had maximised (Openbox's
       * has no border): off by that, and saying where it is */
      native_sdl_wait_rect_at(ctx, data, request, window, native_sdl_expect(
            &monitors[0], monitors[0].x + 60, monitors[0].y + 60,
            320, 240, false), true, 2);
      CHECK(!(SDL_GetWindowFlags(secondary) & SDL_WINDOW_MAXIMIZED));
      CHECK(window->presentable);
   }

   native_sdl_gl(ctx, data, window);
   CHECK(native_sdl_main_intact(data, &main_state));

   native_sdl_main_resize(ctx, data);

   /* Focus in and out */
   if (mapped && native_sdl_focus(ctx, data, request, window, true))
      CHECK(native_sdl_focus(ctx, data, request, window, false));

   /* Close and focus loss outlast a flood the consumer has no room for */
   for (i = 0; i < 128; i++)
      native_sdl_push(secondary, NATIVE_SDL_UNFOCUS, 0, 0);
   native_sdl_push(secondary, NATIVE_SDL_CLOSE, 0, 0);
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
   /* Still there, for its owner to destroy */
   CHECK(SDL_GetWindowFromID(SDL_GetWindowID(secondary)) == secondary);
   CHECK(native_sdl_main_intact(data, &main_state));
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));

   /* What is still queued for the closed window reaches nothing: not
    * the window that replaces it, and not the main window's handling,
    * where the close would quit */
   native_sdl_push(secondary, NATIVE_SDL_RESIZE, 500, 400);
   native_sdl_push(secondary, NATIVE_SDL_CLOSE, 0, 0);
   last = request->generation;
   native_sdl_replace(ctx, data, request, window);
   CHECK(request->generation > last);
   seen = native_window_settle(ctx, data, request, 50000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->presentable);
   CHECK(window->dims == request->settings.dims);
   secondary = (SDL_Window*)window->surface;

   /* Closing the focused window takes its focus with it */
   if (mapped && native_sdl_focus(ctx, data, request, window, true))
   {
      native_sdl_push(secondary, NATIVE_SDL_CLOSE, 0, 0);
      seen = native_window_drain(ctx, data, request);
      CHECK(seen & NATIVE_EVENT(CLOSE));
      CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
      CHECK(!window->focused && !window->presentable);
      /* ... and the closed window reports nothing more */
      SDL_SetWindowSize(secondary, 200, 200);
      native_sdl_activate(secondary, request->screen, false);
      native_sdl_activate(primary, request->screen, true);
      CHECK(!native_window_settle(ctx, data, request, 100000));
      native_sdl_replace(ctx, data, request, window);
      secondary = (SDL_Window*)window->surface;
   }

   /* Closed by the window system: CLOSE, the window still there, and no
    * quit while the main window is up */
   if (mapped && native_sdl_close(secondary, request->screen, false))
   {
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(SDL_GetWindowFromID(SDL_GetWindowID(secondary)) == secondary);
      CHECK(native_sdl_main_intact(data, &main_state));
      native_sdl_replace(ctx, data, request, window);
      secondary = (SDL_Window*)window->surface;
   }

   /* The main window's own close still quits with a secondary up, which
    * keeps SDL from quitting by itself; the secondary stays as it is */
   if (     (placed || ctx->windows->bind_gl)
         && native_sdl_close(primary, request->screen, true))
   {
      seen = 0;
      CHECK(native_sdl_wait_quit(ctx, data, request, &seen));
      CHECK(!(seen & NATIVE_EVENT(CLOSE)));
      CHECK(window->presentable);
      CHECK(native_sdl_main_intact(data, &main_state));
   }

   /* ... and a close request SDL took while the secondary was up, so
    * sent no quit for, still quits once the secondary has gone */
   if (     (placed || ctx->windows->bind_gl)
         && native_sdl_close(primary, request->screen, true))
   {
      unsigned dims = 0;
      bool quit     = false;
      bool resize   = false;
      CHECK(native_sdl_close_queued(primary));
      ctx->windows->destroy(data, window);
      ctx->check_window(data, &quit, &resize, &dims);
      CHECK(quit);
   }

   /* Refused requests leave nothing behind and nothing retries them */
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL);
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
   CHECK(native_sdl_titled(request->screen) == 0);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_sdl_titled(request->screen) == 1);
   /* Made where its saved bounds are, frame or not */
   if (placed)
      native_sdl_wait_rect_at(ctx, data, request, window, native_sdl_expect(
            &monitors[0], VIDEO_POS_X(request->settings.pos),
            VIDEO_POS_Y(request->settings.pos), 320, 240, false), false, 0);

   /* Made fullscreen on a chosen monitor; then a window with automatic
    * bounds: the main window's size, centred on its monitor */
   ctx->windows->destroy(data, window);
   request->generation          = video_driver_window_generation();
   request->settings.fullscreen = true;
   request->settings.monitor    = 1;
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   native_sdl_map(ctx, data, window);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors, 0, true,
         true);
   request->settings.fullscreen = false;
   request->settings.monitor    = 0;
   request->settings.dims       = 0;
   ctx->windows->update(data, window, request);
   native_sdl_wait_fullscreen(ctx, data, request, window, monitors,
         0, false, false);
   if (placed)
   {
      int w, h;
      SDL_GetWindowSize(primary, &w, &h);
      native_sdl_wait_rect(ctx, data, request, window, native_sdl_expect(
            &home, 0, 0, w, h, true));
   }
   request->settings.dims       = VIDEO_SCALE_PACK(320, 240);
   ctx->windows->update(data, window, request);
   native_window_wait(ctx, data, request, window, 0,
         VIDEO_SCALE_PACK(320, 240));
   CHECK(native_sdl_main_intact(data, &main_state));

   if (ctx->windows->bind_gl)
   {
      native_sdl_gl_failures(ctx, data, request, window);
      CHECK(native_sdl_main_intact(data, &main_state));
   }
}
#endif

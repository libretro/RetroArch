/* The SDL contexts' native-window lifecycle for video_screens_test.c,
 * #included there. A build has SDL 2 or SDL 3, never both. */
#ifdef NATIVE_SDL
#include <encodings/utf.h>
#ifdef HAVE_SDL3
#define NATIVE_SDL_KIND    VIDEO_WINDOW_NATIVE_SDL3
#define NATIVE_SDL_CLOSE   SDL_EVENT_WINDOW_CLOSE_REQUESTED
#define NATIVE_SDL_MOVED   SDL_EVENT_WINDOW_MOVED
#define NATIVE_SDL_UNFOCUS SDL_EVENT_WINDOW_FOCUS_LOST
#define NATIVE_SDL_RESIZE  SDL_EVENT_WINDOW_RESIZED
typedef sdl3_video_window_t native_sdl_state_t;
#else
#define NATIVE_SDL_KIND    VIDEO_WINDOW_NATIVE_SDL2
#define NATIVE_SDL_CLOSE   SDL_WINDOWEVENT_CLOSE
#define NATIVE_SDL_MOVED   SDL_WINDOWEVENT_MOVED
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
      CHECK(native_sdl_activate(primary,
               video_window_title_screen(request), true));
      while (window->focused && cpu_features_get_time_usec() < deadline)
      {
         seen |= native_window_drain(ctx, data, request);
         retro_sleep(2);
      }
   }
   if (!native_sdl_activate(on ? (SDL_Window*)window->surface : primary,
            video_window_title_screen(request), !on))
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

typedef struct native_sdl_monitor
{
   char name[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   unsigned dims;
   bool own;
} native_sdl_monitor_t;

/* SDL's displays in its order, as many as the list keeps, with the one
 * SDL has @primary on */
static unsigned native_sdl_monitors(SDL_Window *primary,
      native_sdl_monitor_t *out)
{
   SDL_Rect r;
   const char *name;
   unsigned n = 0;
#ifdef HAVE_SDL3
   int i, count       = 0;
   SDL_DisplayID own  = SDL_GetDisplayForWindow(primary);
   SDL_DisplayID *ids = SDL_GetDisplays(&count);
   for (i = 0; ids && i < count && n < VIDEO_WINDOW_DISPLAYS_MAX; i++, n++)
   {
      memset(&out[n], 0, sizeof(out[n]));
      if ((name = SDL_GetDisplayName(ids[i])))
         utf8cpy(out[n].name, sizeof(out[n].name), name,
               sizeof(out[n].name));
      if (SDL_GetDisplayBounds(ids[i], &r) && VIDEO_SCALE_FITS(r.w, r.h))
         out[n].dims = VIDEO_SCALE_PACK(r.w, r.h);
      out[n].own = ids[i] == own;
   }
   SDL_free(ids);
#else
   int i;
   int count = SDL_GetNumVideoDisplays();
   int own   = SDL_GetWindowDisplayIndex(primary);
   for (i = 0; i < count && n < VIDEO_WINDOW_DISPLAYS_MAX; i++, n++)
   {
      memset(&out[n], 0, sizeof(out[n]));
      if ((name = SDL_GetDisplayName(i)))
         utf8cpy(out[n].name, sizeof(out[n].name), name,
               sizeof(out[n].name));
      if (SDL_GetDisplayBounds(i, &r) == 0 && VIDEO_SCALE_FITS(r.w, r.h))
         out[n].dims = VIDEO_SCALE_PACK(r.w, r.h);
      out[n].own = i == own;
   }
#endif
   return n;
}

/* The screens' Display setting lists SDL's displays by their names,
 * with no model, and with @own the main window's as SDL has it now */
static bool native_sdl_monitors_match(SDL_Window *primary,
      const video_window_display_t *got, unsigned n, bool own)
{
   native_sdl_monitor_t want[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned i;
   unsigned owned = 0;
   if (n != native_sdl_monitors(primary, want))
      return false;
   for (i = 0; i < n; i++)
   {
      if (     got[i].ordinal != i + 1
            || got[i].dims != want[i].dims
            || !string_is_equal(got[i].name, want[i].name)
            || got[i].model[0]
            || (own && got[i].own != want[i].own))
         return false;
      if (got[i].own)
         owned++;
   }
   return owned <= 1;
}

/* Reads the list as the app thread does, pumping until it matches */
static unsigned native_sdl_monitors_wait(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      video_window_display_t *got)
{
   native_sdl_monitor_t want[VIDEO_WINDOW_DISPLAYS_MAX];
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   SDL_Window *primary   = native_sdl_main(data);
   unsigned n;
   for (;;)
   {
      n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
      if (native_sdl_monitors_match(primary, got, n, true))
         return n;
      if (cpu_features_get_time_usec() > deadline)
         break;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   fprintf(stderr, "native sdl: %u monitor(s) published for %u\n", n,
         native_sdl_monitors(primary, want));
   CHECK(!"published monitors");
   return n;
}

/* KWin sends the main window to SDL's display @to, and this pumps until
 * SDL has it there; false without KWin, or when the window never had a
 * frame to be mapped with */
static bool native_sdl_kwin_send(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, unsigned to)
{
   char script[256];
   SDL_Rect r;
   retro_time_t deadline;
   SDL_Window *primary = native_sdl_main(data);
#ifdef HAVE_SDL3
   int count           = 0;
   SDL_DisplayID *ids  = SDL_GetDisplays(&count);
   bool known          = ids && (int)to < count
         && SDL_GetDisplayBounds(ids[to], &r);
   SDL_free(ids);
#else
   bool known          = SDL_GetDisplayBounds((int)to, &r) == 0;
#endif
   if (!known)
      return false;
   snprintf(script, sizeof(script),
         "var o = workspace.screens;"
         " for (var i = 0; m && i < o.length; i++)"
         " if (o[i].geometry.x == %d && o[i].geometry.y == %d)"
         " workspace.sendClientToScreen(m, o[i]);", r.x, r.y);
   if (!native_sdl_kwin(video_window_title_screen(request), script))
      return false;
   deadline = cpu_features_get_time_usec() + 3000000;
   while (native_sdl_display_of(primary) != to)
   {
      if (cpu_features_get_time_usec() > deadline)
      {
         fprintf(stderr, "native sdl: KWin did not move the main window\n");
         return false;
      }
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   return true;
}

/* SDL x11 takes a move when the server answers it */
static void native_sdl_on_display(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, unsigned to)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   SDL_Window *primary   = native_sdl_main(data);
   while (     native_sdl_display_of(primary) != to
         && cpu_features_get_time_usec() < deadline)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   CHECK(native_sdl_display_of(primary) == to);
}

/* The displays as the screens' Display setting lists them, republished
 * when one changes or the main window goes to another, and only then */
static void native_sdl_monitors_check(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request)
{
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   SDL_Event event;
   unsigned i, n, restore;
   int x, y;
   video_driver_state_t *video_st = video_state_get_ptr();
   SDL_Window *primary            = native_sdl_main(data);
   int from                       = -1;
   n = native_sdl_monitors_wait(ctx, data, request, got);
   for (i = 0; i < n; i++)
   {
      char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      CHECK(got[i].valid && got[i].on && got[i].physical && got[i].monitor);
      if (got[i].own)
         from = (int)i;
      video_windows_display_key(got, i, key, sizeof(key));
      fprintf(stderr, "native sdl: monitor %u \"%s\"%s\n", got[i].ordinal,
            key, got[i].own ? ", main window" : "");
      CHECK(key[0] != '\0');
   }

   /* A display event and a main window move that change nothing:
    * nothing is republished */
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   restore = video_st->windows_restore;
   memset(&event, 0, sizeof(event));
#ifdef HAVE_SDL3
   event.type              = SDL_EVENT_DISPLAY_ADDED;
   event.display.displayID = SDL_GetDisplayForWindow(primary);
   CHECK(SDL_PushEvent(&event) > 0);
#elif SDL_VERSION_ATLEAST(2, 0, 14)
   event.type              = SDL_DISPLAYEVENT;
   event.display.event     = SDL_DISPLAYEVENT_CONNECTED;
   CHECK(SDL_PushEvent(&event) > 0);
#endif
   SDL_GetWindowPosition(primary, &x, &y);
   native_sdl_push(primary, NATIVE_SDL_MOVED, x, y);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(video_st->windows_restore == restore);
   CHECK(native_sdl_monitors_match(primary, got, n, true));

   /* The main window's display follows it */
   if (n >= 2 && from >= 0 && native_sdl_driver("x11"))
   {
      SDL_Rect r;
      unsigned to = from ? 0 : 1;
#ifdef HAVE_SDL3
      int count          = 0;
      SDL_DisplayID *ids = SDL_GetDisplays(&count);
      CHECK(ids && SDL_GetDisplayBounds(ids[to], &r));
      SDL_free(ids);
#else
      CHECK(SDL_GetDisplayBounds((int)to, &r) == 0);
#endif
      SDL_SetWindowPosition(primary, r.x + 40, r.y + 40);
      native_sdl_on_display(ctx, data, request, to);
      native_sdl_monitors_wait(ctx, data, request, got);
      CHECK(got[to].own);
      CHECK(video_st->windows_restore != restore);
      SDL_SetWindowPosition(primary, x, y);
      native_sdl_on_display(ctx, data, request, (unsigned)from);
      native_sdl_monitors_wait(ctx, data, request, got);
      CHECK(got[from].own);
   }
   else if (n >= 2 && from >= 0 && native_sdl_driver("wayland")
         && native_sdl_kwin_send(ctx, data, request, from ? 0 : 1))
   {
      native_sdl_monitors_wait(ctx, data, request, got);
      CHECK(got[from ? 0 : 1].own);
      CHECK(video_st->windows_restore != restore);
      CHECK(native_sdl_kwin_send(ctx, data, request, (unsigned)from));
      native_sdl_monitors_wait(ctx, data, request, got);
      CHECK(got[from].own);
   }
   else
      fprintf(stderr, "native sdl: the main window going to another"
            " monitor not checked\n");
}

/* Where SDL's driver cannot make screen windows (cocoa, offscreen),
 * the context's table is not usable, so the frame keeps the screens in
 * the main window. False on the drivers that can. */
static bool native_sdl_no_windows(const gfx_ctx_driver_t *ctx, void *data)
{
   bool usable;
   if (     native_sdl_driver("x11") || native_sdl_driver("wayland")
         || native_sdl_driver("windows"))
      return false;
   fprintf(stderr, "native sdl: %s, no screen windows\n",
         SDL_GetCurrentVideoDriver());
   usable = video_windows_usable(ctx->windows);
   CHECK(ctx->windows != NULL && !usable);
   ctx->destroy(data);
   /* As gl2 and vulkan advertise screen windows */
   window_test_begin();
   window_test_flags = 1u << GFX_CTX_FLAGS_VIDEO_VIEWS;
   if (usable)
      window_test_flags |= 1u << GFX_CTX_FLAGS_VIDEO_WINDOWS;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_handed == NULL);
   window_test_end();
   return true;
}

/* Once the context is gone, its list is too. Then another driver's
 * goes up, which the next context's must replace. */
static void native_sdl_torn_down(const gfx_ctx_driver_t *ctx, bool again)
{
   static video_window_display_t foreign[1];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 0);
   /* usable() answers from init, with no SDL call: SDL video is down */
   if (!SDL_WasInit(SDL_INIT_VIDEO))
      CHECK(video_windows_usable(ctx->windows));
   else
      fprintf(stderr, "native sdl: video still up, the stored answer"
            " not checked\n");
   if (!again)
      return;
   memset(foreign, 0, sizeof(foreign));
   foreign[0].ordinal = 1;
   foreign[0].valid   = foreign[0].on = foreign[0].physical = true;
   foreign[0].monitor = true;
   strlcpy(foreign[0].name, "foreign", sizeof(foreign[0].name));
   video_driver_publish_window_displays(foreign, 1);
}

/* Whether a window manager or compositor is there to maximise it */
static bool native_sdl_managed(SDL_Window *win)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   if (native_sdl_x11(win, &dpy, &xwin))
   {
      Atom type;
      int format;
      unsigned long count, after;
      unsigned char *prop = NULL;
      bool managed        = XGetWindowProperty(dpy, DefaultRootWindow(dpy),
            XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False), 0, 1,
            False, XA_WINDOW, &type, &format, &count, &after, &prop)
         == Success && count == 1;
      if (prop)
         XFree(prop);
      return managed;
   }
#endif
   return native_sdl_driver("wayland");
}

/* Pumps until a GEOMETRY with bounds to save arrives; false at the
 * deadline */
static bool native_sdl_wait_save(const gfx_ctx_driver_t *ctx, void *data,
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

/* Nothing saved since '*saves', else what was; counts on from there */
static void native_sdl_no_save(const char *what, unsigned *saves)
{
   if (native_window_saves != *saves)
      fprintf(stderr, "native sdl: %s saved %u, the last %s%d,%d "
            "%ux%u\n", what, native_window_saves - *saves,
            native_window_saved.save_moved ? "moved " : "",
            VIDEO_POS_X(native_window_saved.save_pos),
            VIDEO_POS_Y(native_window_saved.save_pos),
            VIDEO_SCALE_W(native_window_saved.save_dims),
            VIDEO_SCALE_H(native_window_saved.save_dims));
   CHECK(native_window_saves == *saves);
   *saves = native_window_saves;
}

/* Maximised or restored by the window manager, asked from a connection
 * of the fixture's own as a title bar would: SDL has nothing in flight.
 * False where SDL is not on X11. */
static bool native_sdl_wm_maximise(SDL_Window *win, bool on)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   XEvent event;
   Display *fix = NULL;
   if (native_sdl_x11(win, &dpy, &xwin))
      fix = XOpenDisplay(DisplayString(dpy));
   if (!fix)
      return false;
   memset(&event, 0, sizeof(event));
   event.xclient.type         = ClientMessage;
   event.xclient.window       = xwin;
   event.xclient.message_type = XInternAtom(fix, "_NET_WM_STATE", False);
   event.xclient.format       = 32;
   /* _NET_WM_STATE_ADD or _REMOVE */
   event.xclient.data.l[0]    = on ? 1 : 0;
   event.xclient.data.l[1]    = (long)XInternAtom(fix,
         "_NET_WM_STATE_MAXIMIZED_VERT", False);
   event.xclient.data.l[2]    = (long)XInternAtom(fix,
         "_NET_WM_STATE_MAXIMIZED_HORZ", False);
   event.xclient.data.l[3]    = 1;
   XSendEvent(fix, DefaultRootWindow(fix), False,
         SubstructureRedirectMask | SubstructureNotifyMask, &event);
   XCloseDisplay(fix);
   return true;
#else
   return false;
#endif
}

/* Pumped until SDL says it is maximised, or not */
static void native_sdl_wait_maximised(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request, SDL_Window *win,
      bool on)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (     !(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) != !on
            && cpu_features_get_time_usec() < deadline)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   CHECK(!(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) == !on);
}

/* Maximised or restored by SDL, or with 'wm' by the window manager
 * alone, pumped until SDL says so */
static void native_sdl_maximise(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, SDL_Window *win, bool on,
      bool wm)
{
   if (wm)
      CHECK(native_sdl_wm_maximise(win, on));
   else if (on)
      SDL_MaximizeWindow(win);
   else
      SDL_RestoreWindow(win);
   native_sdl_wait_maximised(ctx, data, request, win, on);
}

/* A manager that configures, and sets _NET_WM_STATE a pump later, from a
 * connection of the fixture's own: the maximised bounds are not saved.
 * No manager may be running. */
static void native_sdl_max_first(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, video_window_native_t *window,
      const SDL_Rect *home, unsigned *saves)
{
#ifdef HAVE_X11
   Display *dpy;
   Window xwin;
   Atom state, max[2];
   int x, y;
   Display *fix    = NULL;
   SDL_Window *win = (SDL_Window*)window->surface;
   unsigned dims   = window->dims;
   if (native_sdl_x11(win, &dpy, &xwin))
      fix = XOpenDisplay(DisplayString(dpy));
   CHECK(fix != NULL);
   if (!fix)
      return;
   SDL_GetWindowPosition(win, &x, &y);
   state  = XInternAtom(fix, "_NET_WM_STATE", False);
   max[0] = XInternAtom(fix, "_NET_WM_STATE_MAXIMIZED_VERT", False);
   max[1] = XInternAtom(fix, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
   XMoveResizeWindow(fix, xwin, home->x, home->y, home->w, home->h);
   XSync(fix, False);
   native_window_wait(ctx, data, request, window, 0,
         VIDEO_SCALE_PACK(home->w, home->h));
   XChangeProperty(fix, xwin, state, XA_ATOM, 32, PropModeReplace,
         (unsigned char*)max, 2);
   XSync(fix, False);
   native_sdl_wait_maximised(ctx, data, request, win, true);
   native_window_settle(ctx, data, request, 200000);
   native_sdl_no_save("maximised a pump after its bounds", saves);
   XDeleteProperty(fix, xwin, state);
   XMoveResizeWindow(fix, xwin, x, y, VIDEO_SCALE_W(dims),
         VIDEO_SCALE_H(dims));
   XSync(fix, False);
   native_sdl_wait_maximised(ctx, data, request, win, false);
   native_window_wait(ctx, data, request, window, 0, dims);
   native_window_settle(ctx, data, request, 200000);
   native_sdl_no_save("restored from that", saves);
   XCloseDisplay(fix);
#endif
}

/* The user's bounds are saved, and neither RetroArch's nor the window
 * manager's are. Once a process; leaves the window made anew from
 * 'request'. */
static void native_sdl_saves(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      const SDL_Rect *home, bool mapped)
{
   static bool done;
   video_window_request_t r;
   video_window_event_t saved;
   SDL_Window *win;
   native_sdl_state_t *state;
   unsigned i, saves;
   float density;
   bool placed         = !native_sdl_driver("wayland");
   bool x11            = native_sdl_driver("x11");
   int x               = home->x;
   int y               = home->y;
#ifdef HAVE_SDL3
   int slack           = 1;
#else
   int slack           = 0;
#endif

   if (done)
      return;
   done                  = true;
   r                     = *request;
   r.settings.monitor    = 0;
   r.settings.fullscreen = false;
   r.settings.pos        = VIDEO_POS_PACK(x + 200, y + 150);
   r.settings.dims       = VIDEO_SCALE_PACK(400, 300);
   r.settings.pos_set    = placed;
   native_sdl_replace(ctx, data, &r, window);
   win     = (SDL_Window*)window->surface;
   density = native_sdl_density(win);
   saves   = native_window_saves;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_sdl_no_save("settling", &saves);

   /* A drag */
   if (placed)
   {
      SDL_SetWindowPosition(win, x + 260, y + 170);
      CHECK(native_sdl_wait_save(ctx, data, &r));
      CHECK(native_window_saved.save_moved);
      CHECK(native_window_saved.save_pos == VIDEO_POS_PACK(x + 260, y + 170));
      CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(400, 300));
   }

   /* A resize in place: the size alone, once. Where the compositor
    * places windows, never a position. */
   SDL_SetWindowSize(win, (int)(420 / density), (int)(310 / density));
   CHECK(native_sdl_wait_save(ctx, data, &r));
   CHECK(!native_window_saved.save_moved);
   CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(420, 310));
   saves = native_window_saves;
   native_window_settle(ctx, data, &r, 200000);
   native_sdl_no_save("resized again", &saves);

   /* Maximised and restored, where something does it: nothing, each
    * time. On X, the last three by the window manager alone. */
   if (mapped && native_sdl_managed(win))
      for (i = 0; i < (x11 ? 6u : 3u); i++)
      {
         bool wm = i >= 3;
         native_sdl_maximise(ctx, data, &r, win, true, wm);
         native_window_settle(ctx, data, &r, 300000);
         CHECK(window->dims != VIDEO_SCALE_PACK(420, 310));
         native_sdl_no_save(wm ? "maximised by the manager" : "maximised",
               &saves);
         native_sdl_maximise(ctx, data, &r, win, false, wm);
         native_window_wait(ctx, data, &r, window, 0,
               VIDEO_SCALE_PACK(420, 310));
         native_window_settle(ctx, data, &r, 200000);
         native_sdl_no_save(wm ? "restored by the manager" : "restored",
               &saves);
      }
   else if (mapped && x11)
      native_sdl_max_first(ctx, data, &r, window, home, &saves);
   else
      fprintf(stderr, "native sdl: nothing here maximises, so no "
            "maximised checks\n");

   /* Settled again, the user's moves are saved */
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_sdl_no_save("settling again", &saves);
   if (placed)
      SDL_SetWindowPosition(win, x + 280, y + 180);
   else
      SDL_SetWindowSize(win, (int)(400 / density), (int)(300 / density));
   CHECK(native_sdl_wait_save(ctx, data, &r));
   CHECK(native_window_saved.save_moved == placed);
   CHECK(!placed
         || native_window_saved.save_pos == VIDEO_POS_PACK(x + 280, y + 180));
   CHECK(native_window_saved.save_dims == (placed
            ? VIDEO_SCALE_PACK(420, 310) : VIDEO_SCALE_PACK(400, 300)));
   saved = native_window_saved;
   saves = native_window_saves;

   /* Fullscreen, moved: nothing */
   r.settings.fullscreen = true;
   native_sdl_replace(ctx, data, &r, window);
   win = (SDL_Window*)window->surface;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   if (placed)
      SDL_SetWindowPosition(win, x + 100, y + 100);
   SDL_SetWindowSize(win, (int)(300 / density), (int)(200 / density));
   native_window_settle(ctx, data, &r, 300000);
   native_sdl_no_save("fullscreen", &saves);
   r.settings.fullscreen = false;

   /* Moved at once after it was made, as a window manager places it */
   ctx->windows->destroy(data, window);
   r.generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, &r, window));
   win = (SDL_Window*)window->surface;
   if (placed)
      SDL_SetWindowPosition(win, x + 240, y + 190);
   else
      SDL_SetWindowSize(win, (int)(380 / density), (int)(280 / density));
   native_sdl_map(ctx, data, window);
   native_window_settle(ctx, data, &r, 600000);
   native_sdl_no_save("moved at once", &saves);

   /* Saved bounds make the window they were saved from, and it reports
    * them: SDL 3's size in pixels to one pixel, and exactly the second
    * time round */
   r.settings.pos     = saved.save_pos;
   r.settings.dims    = saved.save_dims;
   r.settings.pos_set = placed;
   for (i = 0; i < 2; i++)
   {
      int s = i ? 0 : slack;
      native_sdl_replace(ctx, data, &r, window);
      native_window_settle(ctx, data, &r,
            VIDEO_WINDOW_SETTLE_USEC + 100000);
      state = (native_sdl_state_t*)window->state;
      if (     window->dims != r.settings.dims
            || (placed && state && state->common.pos != r.settings.pos))
         fprintf(stderr, "native sdl: remade at %d,%d %ux%u from %d,%d "
               "%ux%u\n", state ? VIDEO_POS_X(state->common.pos) : 0,
               state ? VIDEO_POS_Y(state->common.pos) : 0,
               VIDEO_SCALE_W(window->dims), VIDEO_SCALE_H(window->dims),
               VIDEO_POS_X(r.settings.pos), VIDEO_POS_Y(r.settings.pos),
               VIDEO_SCALE_W(r.settings.dims),
               VIDEO_SCALE_H(r.settings.dims));
      CHECK(!placed || (state && state->common.pos == r.settings.pos));
      CHECK(abs((int)VIDEO_SCALE_W(window->dims)
               - (int)VIDEO_SCALE_W(r.settings.dims)) <= s);
      CHECK(abs((int)VIDEO_SCALE_H(window->dims)
               - (int)VIDEO_SCALE_H(r.settings.dims)) <= s);
      native_sdl_no_save("remade", &saves);
      r.settings.dims = window->dims;
   }

   native_sdl_replace(ctx, data, request, window);
   /* It came up where it was put, which no GEOMETRY says */
   state = (native_sdl_state_t*)window->state;
   if (state)
      native_window_pos = state->common.pos;
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
   /* Published by set_video_mode, before anything pumped */
   {
      video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
      unsigned n = video_driver_get_window_displays(got,
            VIDEO_WINDOW_DISPLAYS_MAX);
      fprintf(stderr, "native sdl: %u monitor(s) published at init%s\n", n,
            n && string_is_equal(got[0].name, "foreign")
            ? ", the foreign list" : "");
      CHECK(native_sdl_monitors_match(primary, got, n, false));
   }
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
   CHECK(native_sdl_titled(video_window_title_screen(request)) == 1);
   native_window_titles(SDL_GetWindowTitle(secondary),
         SDL_GetWindowTitle(primary), video_window_title_screen(request));
#ifdef HAVE_X11
   {
      Display *dpy, *main_dpy;
      Window xwin, main_xwin;
      if (     native_sdl_x11(secondary, &dpy, &xwin)
            && native_sdl_x11(primary, &main_dpy, &main_xwin))
         native_x11_titles(dpy, main_xwin, xwin,
               video_window_title_screen(request));
   }
#endif
#ifdef NATIVE_WIN32
   if (native_sdl_hwnd(secondary) && native_sdl_hwnd(primary))
      native_w32_titles(native_sdl_hwnd(primary), native_sdl_hwnd(secondary),
            video_window_title_screen(request));
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
#ifdef HAVE_VULKAN
   if (strcmp(ctx->ident, "vk_sdl3") == 0)
      native_vk_output(ctx, data, window);
#endif
   /* Shown without taking the focus from the main window, where the
    * window system leaves that to the client */
   if (native_sdl_driver("windows"))
      CHECK(!window->focused);
   /* as the window manager has settled the main window */
   native_sdl_main_get(data, &main_state);
   density = native_sdl_density(secondary);
   native_sdl_saves(ctx, data, request, window, &home, mapped);
   secondary = (SDL_Window*)window->surface;
   CHECK(native_sdl_main_intact(data, &main_state));

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
      CHECK(native_sdl_unminimise(secondary,
            video_window_title_screen(request)));
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
      native_sdl_activate(secondary, video_window_title_screen(request),
            false);
      native_sdl_activate(primary, video_window_title_screen(request), true);
      CHECK(!native_window_settle(ctx, data, request, 100000));
      native_sdl_replace(ctx, data, request, window);
      secondary = (SDL_Window*)window->surface;
   }

   /* Closed by the window system: CLOSE, the window still there, and no
    * quit while the main window is up */
   if (mapped && native_sdl_close(secondary,
            video_window_title_screen(request), false))
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
         && native_sdl_close(primary, video_window_title_screen(request),
               true))
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
         && native_sdl_close(primary, video_window_title_screen(request),
               true))
   {
      unsigned dims = 0;
      bool quit     = false;
      bool resize   = false;
      CHECK(native_sdl_close_queued(primary));
      ctx->windows->destroy(data, window);
      ctx->check_window(data, &quit, &resize, &dims);
      CHECK(quit);
   }

   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL);

   /* Game Screen 1's window on a spare id: titled for its screen */
   {
      video_window_request_t spare;
      video_window_native_t one;
      native_window_spare(request, &spare);
      memset(&one, 0, sizeof(one));
      CHECK(ctx->windows->create(data, &spare, &one));
      CHECK(native_sdl_titled(0) == 1);
      CHECK(native_sdl_titled(spare.screen) == 0);
      if (one.surface)
         native_window_titles(SDL_GetWindowTitle((SDL_Window*)one.surface),
               SDL_GetWindowTitle(primary), 0);
      ctx->windows->destroy(data, &one);
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
   CHECK(native_sdl_titled(video_window_title_screen(request)) == 0);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_sdl_titled(video_window_title_screen(request)) == 1);
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
   native_sdl_monitors_check(ctx, data, request);
}
#endif

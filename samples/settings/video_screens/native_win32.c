/* The Win32 contexts' native-window lifecycle for video_screens_test.c,
 * and the native table a Direct3D driver uses without a context;
 * #included there. */
#ifdef NATIVE_WIN32
#define NATIVE_W32_STYLE (WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS \
      | WS_CLIPCHILDREN)

static bool native_w32_ident(const char *ident)
{
   return    strcmp(ident, "wgl") == 0
          || strcmp(ident, "vk_w") == 0
          || strcmp(ident, "d3d") == 0;
}

/* The run loop's pump, which is the only one a window has when video
 * is not threaded */
static void native_w32_pump(void *data)
{
   ui_companion_win32.application->process_events();
}

/* What stands in for a Direct3D driver: the real main window, made the
 * way D3D11 and D3D12 make theirs (the D3D family, tagged so that it is
 * kept across the restart), no device and no swap chain. The owner is
 * a byte standing in for the driver's video data. */
static gfx_ctx_driver_t native_w32_d3d;
static char native_w32_d3d_owner;

static bool native_w32_d3d_set_video_mode(void *data, unsigned dims,
      bool fullscreen)
{
   return win32_set_video_mode(data, dims, fullscreen);
}

/* As the drivers' free does: the windows first, then the main window */
static void native_w32_d3d_destroy(void *data)
{
   win32_video_windows_free(data, win32_video_window_driver.destroy);
   if (!win32_window_keep())
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }
}

static const gfx_ctx_driver_t *native_w32_d3d_init(void **data)
{
   WNDCLASSEX wndclass;
   memset(&wndclass, 0, sizeof(wndclass));
   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = win32_window_proc;
   win32_window_proc_setup(WIN32_WINDOW_D3D);
   win32_window_tag("video_screens_test");
   if (!win32_window_init(&wndclass, true, NULL))
      return NULL;
   memset(&native_w32_d3d, 0, sizeof(native_w32_d3d));
   native_w32_d3d.ident          = "d3d";
   native_w32_d3d.set_video_mode = native_w32_d3d_set_video_mode;
   native_w32_d3d.check_window   = win32_check_window;
   native_w32_d3d.destroy        = native_w32_d3d_destroy;
   native_w32_d3d.windows        = &win32_video_window_driver;
   *data                         = &native_w32_d3d_owner;
   return &native_w32_d3d;
}

/* The main window's state, which a secondary must leave alone */
typedef struct
{
   HWND hwnd;
   RECT rect;
   LONG_PTR style;
   LONG_PTR proc;
   unsigned resize_w;
   unsigned resize_h;
} native_w32_main_t;

static void native_w32_main_get(native_w32_main_t *main_state)
{
   memset(main_state, 0, sizeof(*main_state));
   main_state->hwnd     = main_window.hwnd;
   GetWindowRect(main_window.hwnd, &main_state->rect);
   main_state->style    = GetWindowLongPtr(main_window.hwnd, GWL_STYLE);
   main_state->proc     = GetWindowLongPtr(main_window.hwnd, GWLP_WNDPROC);
   main_state->resize_w = g_win32_resize_width;
   main_state->resize_h = g_win32_resize_height;
}

static bool native_w32_main_intact(const native_w32_main_t *was)
{
   native_w32_main_t now;
   native_w32_main_get(&now);
   return    now.hwnd && now.hwnd == was->hwnd
          && IsWindow(now.hwnd) && (now.style & WS_VISIBLE)
          && EqualRect(&now.rect, &was->rect)
          && now.style    == was->style
          && now.proc     == was->proc
          && now.resize_w == was->resize_w
          && now.resize_h == was->resize_h
          && video_driver_window_get() == (uintptr_t)now.hwnd
          && !(win32_get_flags() & WIN32_CMN_FLAG_QUIT);
}

typedef struct
{
   HMONITOR handle;
   RECT rect;
   RECT work;
} native_w32_monitor_t;

typedef struct
{
   native_w32_monitor_t *out;
   unsigned capacity;
   unsigned count;
} native_w32_monitors_t;

static BOOL CALLBACK native_w32_monitor(HMONITOR handle, HDC dc,
      LPRECT rect, LPARAM data)
{
   MONITORINFO info;
   native_w32_monitors_t *list = (native_w32_monitors_t*)data;
   if (list->count >= list->capacity)
      return FALSE;
   memset(&info, 0, sizeof(info));
   info.cbSize = sizeof(info);
   CHECK(GetMonitorInfo(handle, &info));
   list->out[list->count].handle = handle;
   list->out[list->count].rect   = info.rcMonitor;
   list->out[list->count].work   = info.rcWork;
   list->count++;
   return TRUE;
}

/* In EnumDisplayMonitors' order: monitor N is entry N - 1 */
static unsigned native_w32_monitors(native_w32_monitor_t *out,
      unsigned capacity)
{
   native_w32_monitors_t list;
   list.out      = out;
   list.capacity = capacity;
   list.count    = 0;
   EnumDisplayMonitors(NULL, NULL, native_w32_monitor, (LPARAM)&list);
   return list.count;
}

static const native_w32_monitor_t *native_w32_monitor_of(
      const native_w32_monitor_t *monitors, unsigned count, HMONITOR handle)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (monitors[i].handle == handle)
         return &monitors[i];
   return &monitors[0];
}

/* The main window of the lifetime last checked */
static HWND native_w32_main_last;

/* What the screens' Display setting keys @handle by: its connector
 * where Windows says, else its GDI device */
static void native_w32_monitor_name(HMONITOR handle, char *name,
      size_t name_len, char *model, size_t model_len)
{
   MONITORINFOEX info;
   video_window_display_t display;
   memset(&info, 0, sizeof(info));
   memset(&display, 0, sizeof(display));
   info.cbSize = sizeof(info);
   CHECK(GetMonitorInfo(handle, (LPMONITORINFO)&info));
   strlcpy(display.name, info.szDevice, sizeof(display.name));
   win32_display_config_connectors(&display, 1);
   strlcpy(name, display.name, name_len);
   strlcpy(model, display.model, model_len);
}

/* "DP-1", or a GDI device "\\.\DISPLAY1" */
static bool native_w32_name_ok(const char *name)
{
   const char *s = name;
   if (strncmp(name, "\\\\.\\DISPLAY", 11) == 0)
      return name[11] >= '1' && name[11] <= '9';
   while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
      s++;
   if (s == name || *s++ != '-' || *s < '1' || *s > '9')
      return false;
   while (*s >= '0' && *s <= '9')
      s++;
   return *s == '\0';
}

/* The published list is EnumDisplayMonitors' monitors in order, the
 * main window's the one it is on now */
static bool native_w32_monitors_match(const native_w32_monitor_t *monitors,
      unsigned want, const video_window_display_t *got, unsigned n)
{
   unsigned i;
   unsigned owned = 0;
   HMONITOR own   = MonitorFromWindow(main_window.hwnd,
         MONITOR_DEFAULTTONEAREST);
   if (n != want)
      return false;
   for (i = 0; i < n; i++)
   {
      char name[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      char model[VIDEO_WINDOW_DISPLAY_MODEL_SIZE];
      const RECT *rect = &monitors[i].rect;
      native_w32_monitor_name(monitors[i].handle, name, sizeof(name),
            model, sizeof(model));
      if (     got[i].ordinal != i + 1
            || got[i].dims != VIDEO_SCALE_PACK(rect->right - rect->left,
               rect->bottom - rect->top)
            || got[i].own != (monitors[i].handle == own)
            || !string_is_equal(got[i].name, name)
            || !string_is_equal(got[i].model, model))
         return false;
      if (got[i].own)
         owned++;
   }
   return owned == 1;
}

/* Reads the list as the app thread does, pumping until it matches */
static unsigned native_w32_monitors_wait(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      const native_w32_monitor_t *monitors, unsigned want,
      video_window_display_t *got)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   unsigned n;
   for (;;)
   {
      n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
      if (native_w32_monitors_match(monitors, want, got, n))
         return n;
      if (cpu_features_get_time_usec() > deadline)
         break;
      if (request)
         native_window_drain(ctx, data, request);
      else
      {
         unsigned dims = VIDEO_SCALE_PACK(640, 480);
         bool quit     = false;
         bool resize   = false;
         native_w32_pump(data);
         ctx->check_window(data, &quit, &resize, &dims);
      }
      retro_sleep(2);
   }
   fprintf(stderr, "native win32: %u monitor(s) published for %u\n",
         n, want);
   CHECK(!"published monitors");
   return n;
}

/* The monitors as the screens' Display setting lists them, republished
 * when one changes or the main window goes to another, and only then */
static void native_w32_monitors_check(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request)
{
   native_w32_monitor_t monitors[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_display_t nosuch;
   RECT was;
   unsigned i, n, restore;
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned want = native_w32_monitors(monitors, VIDEO_WINDOW_DISPLAYS_MAX);
   int from      = -1;
   n = native_w32_monitors_wait(ctx, data, request, monitors, want, got);
   for (i = 0; i < n; i++)
   {
      char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      CHECK(got[i].valid && got[i].on && got[i].physical && got[i].monitor);
      CHECK(native_w32_name_ok(got[i].name));
      if (got[i].own)
         from = (int)i;
      video_windows_display_key(got, i, key, sizeof(key));
      fprintf(stderr, "native win32: monitor %u \"%s\" model \"%s\","
            " %s%s\n", got[i].ordinal, key, got[i].model,
            strncmp(got[i].name, "\\\\.\\", 4) == 0
            ? "GDI device name" : "DisplayConfig connector",
            got[i].own ? ", main window" : "");
      CHECK(key[0] != '\0');
   }
   memset(&nosuch, 0, sizeof(nosuch));
   strlcpy(nosuch.name, "\\\\.\\NOSUCH", sizeof(nosuch.name));
   win32_display_config_connectors(&nosuch, 1);
   CHECK(string_is_equal(nosuch.name, "\\\\.\\NOSUCH") && !*nosuch.model);

   /* Moved within its monitor, and a display change that changes
    * nothing: nothing is republished */
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   restore = video_st->windows_restore;
   GetWindowRect(main_window.hwnd, &was);
   SetWindowPos(main_window.hwnd, NULL, was.left + 8, was.top + 8, 0, 0,
         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
   native_window_settle(ctx, data, request, 50000);
   SendMessage(main_window.hwnd, WM_DISPLAYCHANGE, 32,
         MAKELPARAM(monitors[0].rect.right - monitors[0].rect.left,
            monitors[0].rect.bottom - monitors[0].rect.top));
   SetWindowPos(main_window.hwnd, NULL, was.left, was.top,
         was.right - was.left, was.bottom - was.top,
         SWP_NOZORDER | SWP_NOACTIVATE);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(video_st->windows_restore == restore);

   /* The main window's monitor follows it */
   if (n >= 2 && from >= 0)
   {
      unsigned to = from ? 0 : 1;
      SetWindowPos(main_window.hwnd, NULL, monitors[to].work.left + 40,
            monitors[to].work.top + 40, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      native_w32_monitors_wait(ctx, data, request, monitors, want, got);
      CHECK(got[to].own);
      CHECK(video_st->windows_restore != restore);
      SetWindowPos(main_window.hwnd, NULL, was.left, was.top, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      native_w32_monitors_wait(ctx, data, request, monitors, want, got);
      CHECK(got[from].own);
   }
   else
      fprintf(stderr, "native win32: the main window going to another"
            " monitor not checked\n");
}

/* The client area in screen coordinates */
static RECT native_w32_client(HWND hwnd)
{
   RECT rect;
   POINT origin;
   origin.x = 0;
   origin.y = 0;
   memset(&rect, 0, sizeof(rect));
   GetClientRect(hwnd, &rect);
   ClientToScreen(hwnd, &origin);
   OffsetRect(&rect, origin.x, origin.y);
   return rect;
}

/* Where a window with saved client bounds x,y w*h belongs on a monitor
 * with work area 'work': its frame moved onto the work area, or centred
 * on it when the bounds are automatic or not on it at all, and no
 * bigger than it. */
static RECT native_w32_expect(const RECT *work, int x, int y,
      unsigned w, unsigned h, bool automatic)
{
   RECT frame, inter, out;
   int left, top, right, bottom, fw, fh;
   frame.left   = x;
   frame.top    = y;
   frame.right  = x + (int)w;
   frame.bottom = y + (int)h;
   AdjustWindowRectEx(&frame, NATIVE_W32_STYLE, FALSE, 0);
   left   = x - frame.left;
   top    = y - frame.top;
   right  = frame.right  - (x + (int)w);
   bottom = frame.bottom - (y + (int)h);
   fw     = frame.right  - frame.left;
   fh     = frame.bottom - frame.top;
   if (fw > work->right - work->left)
      fw = work->right - work->left;
   if (fh > work->bottom - work->top)
      fh = work->bottom - work->top;
   if (automatic || !IntersectRect(&inter, &frame, work))
   {
      frame.left = work->left + (work->right  - work->left - fw) / 2;
      frame.top  = work->top  + (work->bottom - work->top  - fh) / 2;
   }
   else
   {
      if (frame.left < work->left)
         frame.left = work->left;
      if (frame.top < work->top)
         frame.top  = work->top;
      if (frame.left + fw > work->right)
         frame.left = work->right - fw;
      if (frame.top + fh > work->bottom)
         frame.top  = work->bottom - fh;
   }
   out.left   = frame.left + left;
   out.top    = frame.top  + top;
   out.right  = frame.left + fw - right;
   out.bottom = frame.top  + fh - bottom;
   return out;
}

/* Pumps until the client area is 'want', anywhere with an empty one,
 * and the record and the last geometry event say so too. */
static void native_w32_wait_rect(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, RECT want)
{
   RECT rect;
   bool anywhere         = IsRectEmpty(&want);
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      rect = native_w32_client((HWND)window->surface);
      if (anywhere)
         want = rect;
      if (     EqualRect(&rect, &want)
            && window->dims == VIDEO_SCALE_PACK(rect.right - rect.left,
               rect.bottom - rect.top)
            && native_window_pos == VIDEO_POS_PACK(rect.left, rect.top))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: at %ld,%ld %ldx%ld (event %d,%d), "
         "wanted %ld,%ld %ldx%ld\n", (long)rect.left, (long)rect.top,
         (long)(rect.right - rect.left), (long)(rect.bottom - rect.top),
         VIDEO_POS_X(native_window_pos), VIDEO_POS_Y(native_window_pos),
         (long)want.left, (long)want.top, (long)(want.right - want.left),
         (long)(want.bottom - want.top));
   CHECK(!"window bounds");
}

static bool native_w32_fullscreen_style(HWND hwnd)
{
   LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
   return (style & WS_POPUP) && !(style & WS_CAPTION);
}

/* Top-level windows named for 'screen', in any class */
static unsigned native_w32_titled(unsigned screen)
{
   char want[64];
   HWND hwnd      = NULL;
   unsigned found = 0;
   snprintf(want, sizeof(want), "RetroArch - Screen %u", screen + 1);
   while ((hwnd = FindWindowEx(NULL, hwnd, NULL, want)))
      found++;
   return found;
}

/* The titles as Windows has them */
static void native_w32_titles(HWND primary, HWND win, unsigned screen)
{
   char title[128], main_title[128];
   title[0] = main_title[0] = '\0';
   GetWindowText(win, title, sizeof(title));
   GetWindowText(primary, main_title, sizeof(main_title));
   native_window_titles(title, main_title, screen);
}

/* The secondaries' class, as the first window had it */
static char native_w32_class[64];

/* The restore count when a main window was kept: taking it back must
 * publish nothing */
static bool native_w32_kept;
static unsigned native_w32_kept_restore;

/* Once the context is gone: no window left, and no class either. A
 * main window kept for the next driver keeps its monitors up. */
static void native_w32_torn_down(unsigned screen)
{
   WNDCLASSEX has;
   native_w32_monitor_t monitors[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned n = video_driver_get_window_displays(got,
         VIDEO_WINDOW_DISPLAYS_MAX);
   has.cbSize = sizeof(has);
   CHECK(native_w32_titled(screen) == 0);
   CHECK(*native_w32_class);
   CHECK(!GetClassInfoEx(GetModuleHandle(NULL), native_w32_class, &has));
   if (native_w32_main_last && IsWindow(native_w32_main_last))
   {
      CHECK(n == native_w32_monitors(monitors, VIDEO_WINDOW_DISPLAYS_MAX));
      native_w32_kept         = true;
      native_w32_kept_restore = video_state_get_ptr()->windows_restore;
   }
   else
      CHECK(n == 0);
}

#ifdef HAVE_THREADS
typedef struct
{
   const gfx_ctx_driver_t *ctx;
   void *data;
   video_window_request_t request;
   video_window_native_t window;
   bool created;
} native_w32_elsewhere_t;

static void native_w32_create_elsewhere(void *arg)
{
   native_w32_elsewhere_t *task = (native_w32_elsewhere_t*)arg;
   task->created = task->ctx->windows->create(task->data, &task->request,
         &task->window);
}
#endif

#ifdef NATIVE_WGL
/* A secondary whose device context fails fails its window, which is
 * then replaced. Its DC is swapped for one already deleted: a present
 * cannot swap it, and a bind cannot make the context current on it.
 * Each gives CLOSE, with the main window's DC, context and interval
 * back after. */
static void native_wgl_failures(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   typedef BOOL (WINAPI *set_interval_t)(int);
   typedef int (WINAPI *get_interval_t)(void);
   unsigned i;
   HDC primary   = wglGetCurrentDC();
   HGLRC context = wglGetCurrentContext();
   HDC bad       = CreateCompatibleDC(NULL);
   set_interval_t set_interval = (set_interval_t)
      wglGetProcAddress("wglSwapIntervalEXT");
   get_interval_t get_interval = (get_interval_t)
      wglGetProcAddress("wglGetSwapIntervalEXT");
   CHECK(bad != NULL);
   CHECK(DeleteDC(bad));
   for (i = 0; bad && i < 2; i++)
   {
      win32_video_window_t *state = (win32_video_window_t*)window->state;
      HDC good                    = state->hdc;
      CHECK(window->presentable);
      if (i && set_interval && get_interval)
      {
         /* After a good bind, the main window's interval at the
          * secondary's 0, as a driver with one per context leaves it */
         CHECK(ctx->windows->bind_gl(data, window));
         CHECK(wglMakeCurrent(primary, context));
         set_interval(0);
      }
      state->hdc                  = bad;
      if (!i)
         CHECK(!ctx->windows->present_gl(data, window));
      else
         CHECK(!ctx->windows->bind_gl(data, window));
      state->hdc                  = good;
      CHECK(!i || !get_interval || get_interval() == 1);
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(!window->presentable);
      CHECK(!ctx->windows->bind_gl(data, window));
      CHECK(wglGetCurrentDC() == primary);
      CHECK(wglGetCurrentContext() == context);
      ctx->windows->destroy(data, window);
      request->generation = video_driver_window_generation();
      CHECK(ctx->windows->create(data, request, window));
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
   }
}
#endif

/* A drag of a secondary is a modal loop, as a drag of the main window
 * is; a secondary destroyed mid-drag leaves the bookkeeping so that
 * the main window's next drag counts again. */
static void native_w32_sizemove(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   HWND secondary = (HWND)window->surface;
   HWND primary   = main_window.hwnd;
   CHECK(!video_driver_is_threaded());
   CHECK(!win32_message_modal_active());
   SendMessage(secondary, WM_ENTERSIZEMOVE, 0, 0);
   CHECK(win32_message_modal_active());
   SendMessage(secondary, WM_EXITSIZEMOVE, 0, 0);
   CHECK(!win32_message_modal_active());

   SendMessage(secondary, WM_ENTERSIZEMOVE, 0, 0);
   ctx->windows->destroy(data, window);
   CHECK(!IsWindow(secondary));
   CHECK(!win32_message_modal_active());
   SendMessage(primary, WM_ENTERSIZEMOVE, 0, 0);
   CHECK(win32_message_modal_active());
   SendMessage(primary, WM_EXITSIZEMOVE, 0, 0);
   CHECK(!win32_message_modal_active());

   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}

/* A destroyed secondary's freed state, behind a guard page */
typedef struct
{
   ULONG_PTR from;
   ULONG_PTR to;
   ULONG_PTR first_page;
   ULONG_PTR end_page;
   DWORD protect;
   bool watched;
   bool stepping;
   bool read;
} native_w32_freed_t;

static native_w32_freed_t native_w32_freed;

static LONG CALLBACK native_w32_guard(EXCEPTION_POINTERS *info)
{
   const EXCEPTION_RECORD *record = info->ExceptionRecord;
   ULONG_PTR at;
   DWORD was;
   /* Past an access elsewhere on its pages (the pump's own TLS block
    * can share one): guarded again */
   if (     record->ExceptionCode == STATUS_SINGLE_STEP
         && native_w32_freed.stepping)
   {
      native_w32_freed.stepping = false;
      if (native_w32_freed.watched)
         VirtualProtect((void*)native_w32_freed.from,
               sizeof(win32_video_window_t), PAGE_READWRITE | PAGE_GUARD,
               &was);
      return EXCEPTION_CONTINUE_EXECUTION;
   }
   if (     !native_w32_freed.watched
         || record->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION
         || record->NumberParameters < 2)
      return EXCEPTION_CONTINUE_SEARCH;
   at = record->ExceptionInformation[1];
   if (at < native_w32_freed.first_page || at >= native_w32_freed.end_page)
      return EXCEPTION_CONTINUE_SEARCH;
   if (at >= native_w32_freed.from && at < native_w32_freed.to)
      native_w32_freed.read = true;
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) \
      || defined(_M_X64)
   else
   {
      info->ContextRecord->EFlags |= 0x100; /* trap flag */
      native_w32_freed.stepping   = true;
   }
#endif
   return EXCEPTION_CONTINUE_EXECUTION;
}

static void native_w32_watch(const void *state)
{
   SYSTEM_INFO system;
   ULONG_PTR mask;
   GetSystemInfo(&system);
   mask                        = (ULONG_PTR)system.dwPageSize - 1;
   native_w32_freed.from       = (ULONG_PTR)state;
   native_w32_freed.to         = native_w32_freed.from
      + sizeof(win32_video_window_t);
   native_w32_freed.first_page = native_w32_freed.from & ~mask;
   native_w32_freed.end_page   = (native_w32_freed.to + mask) & ~mask;
   native_w32_freed.read       = false;
   /* pages the heap gave back fault on a read anyway */
   native_w32_freed.watched    = VirtualProtect((void*)native_w32_freed.from,
         sizeof(win32_video_window_t), PAGE_READWRITE | PAGE_GUARD,
         &native_w32_freed.protect) != 0;
}

static void native_w32_unwatch(void)
{
   DWORD was;
   if (!native_w32_freed.watched)
      return;
   VirtualProtect((void*)native_w32_freed.from, sizeof(win32_video_window_t),
         native_w32_freed.protect, &was);
   native_w32_freed.watched = false;
}

static bool native_w32_next(void)
{
   MSG msg;
   native_w32_unwatch();
   if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      return false;
   TranslateMessage(&msg);
   DispatchMessage(&msg);
   return true;
}

/* Under the run loop's pump, a secondary's drag yields to the frame,
 * which can destroy the window before the drag resumes; the window
 * procedure then must not read the state that destroy freed. Watched
 * until the pump next asks for a message. */
static void native_w32_sizemove_pumped(const gfx_ctx_driver_t *ctx,
      void *data, video_window_request_t *request,
      video_window_native_t *window)
{
   HWND secondary = (HWND)window->surface;
   const void *state;
   void *handler;
   if (!win32_message_pump(native_w32_next))
   {
      printf("UNAVAILABLE: no message fiber; a drag under the pump is "
            "not checked\n");
      return;
   }
   PostMessage(secondary, WM_ENTERSIZEMOVE, 0, 0);
   CHECK(win32_message_pump(native_w32_next));
   CHECK(win32_message_modal_active());
   state = window->state;
   ctx->windows->destroy(data, window);
   CHECK(!IsWindow(secondary));
   CHECK(!win32_message_modal_active());
   handler = AddVectoredExceptionHandler(1, native_w32_guard);
   CHECK(handler != NULL);
   native_w32_watch(state);
   CHECK(win32_message_pump(native_w32_next));
   native_w32_unwatch();
   if (handler)
      RemoveVectoredExceptionHandler(handler);
   CHECK(!native_w32_freed.read);
   SendMessage(main_window.hwnd, WM_ENTERSIZEMOVE, 0, 0);
   CHECK(win32_message_modal_active());
   SendMessage(main_window.hwnd, WM_EXITSIZEMOVE, 0, 0);
   CHECK(!win32_message_modal_active());

   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
}

/* Focus given to the window and back to the main window */
static void native_w32_focus(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, bool on)
{
   unsigned seen;
   SetFocus(on ? (HWND)window->surface : main_window.hwnd);
   seen = native_window_wait(ctx, data, request, window,
         on ? NATIVE_EVENT(FOCUS_GAIN) : NATIVE_EVENT(FOCUS_LOSS), 0);
   CHECK(window->focused == on);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
}

static void native_w32_lifecycle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   native_w32_monitor_t monitors[8];
   native_w32_main_t main_state;
   const native_w32_monitor_t *home;
   video_window_request_t other;
   video_window_native_t invalid;
   RECT rect, before;
   unsigned i, seen, last;
   HWND secondary = (HWND)window->surface;
   unsigned count = native_w32_monitors(monitors, 8);
   native_w32_main_last = main_window.hwnd;

   fprintf(stderr, "native win32: %u monitor(s)\n", count);
   CHECK(count > 0);
   if (!count)
      return;
   home = native_w32_monitor_of(monitors, count,
         MonitorFromWindow(main_window.hwnd, MONITOR_DEFAULTTONEAREST));
   native_w32_main_get(&main_state);
   CHECK(window->display == (void*)GetModuleHandle(NULL));
   CHECK(secondary && IsWindow(secondary) && secondary != main_window.hwnd);
   CHECK(GetWindowThreadProcessId(secondary, NULL) == GetCurrentThreadId());
   CHECK(GetClassName(secondary, native_w32_class, sizeof(native_w32_class)));
   CHECK(strcmp(native_w32_class, "RetroArch") != 0);
   CHECK(GetWindowLongPtr(secondary, GWLP_WNDPROC)
         != (LONG_PTR)win32_window_proc);
   CHECK(native_w32_titled(request->screen) == 1);
   native_w32_titles(main_window.hwnd, secondary, request->screen);
   /* Shown without taking the focus from the main window */
   CHECK(GetFocus() != secondary);
   /* After a restart, what was queued for the last lifetime's window
    * (a close and a resize) has to reach nothing */
   seen  = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 100000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   /* The main window taken back: its list is still the one up */
   if (native_w32_kept && win32_window_was_taken())
   {
      video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
      unsigned restore;
      video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
      restore = video_state_get_ptr()->windows_restore;
      fprintf(stderr, "native win32: main window taken back,"
            " %u restore(s) since it was kept\n",
            restore - native_w32_kept_restore);
      CHECK(restore == native_w32_kept_restore);
   }
   native_w32_kept = false;
   CHECK(window->dims == request->settings.dims);
   CHECK(window->configured && window->presentable);
   CHECK(native_w32_main_intact(&main_state));

   /* Resized by the user; the events follow where the window is */
   rect.left   = 0;
   rect.top    = 0;
   rect.right  = 400;
   rect.bottom = 300;
   AdjustWindowRectEx(&rect, NATIVE_W32_STYLE, FALSE, 0);
   SetWindowPos(secondary, NULL, 0, 0, rect.right - rect.left,
         rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
         VIDEO_SCALE_PACK(400, 300));
   SetRectEmpty(&rect);
   native_w32_wait_rect(ctx, data, request, window, rect);

   /* A preferred monitor, with saved bounds that are not on it: moved
    * onto it where they reach it, centred where they do not */
   request->settings.monitor = count;
   request->settings.pos     = VIDEO_POS_PACK(-200, -200);
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &monitors[count - 1].work, -200, -200, 320, 240, false));

   /* No preferred monitor: the one the bounds are mostly on, here
    * partly off its left edge */
   request->settings.monitor = 0;
   request->settings.pos     = VIDEO_POS_PACK(home->work.left - 100,
         home->work.top + 60);
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &home->work, home->work.left - 100, home->work.top + 60,
         320, 240, false));

   /* ... and saved bounds on none: the main window's */
   request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &home->work, 0, 0, 320, 240, true));

   /* A saved monitor that is gone: the main window's instead */
   request->settings.monitor = count;
   request->settings.pos     = VIDEO_POS_PACK(monitors[count - 1].work.left
         + 10, monitors[count - 1].work.top + 40);
   ctx->windows->update(data, window, request);
   native_window_settle(ctx, data, request, 50000);
   request->settings.monitor = count + 3;
   request->settings.pos     = VIDEO_POS_PACK(-30000, -30000);
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &home->work, 0, 0, 320, 240, true));
   CHECK(native_w32_main_intact(&main_state));

   /* Moved by the user onto the last monitor */
   rect = native_w32_expect(&monitors[count - 1].work,
         monitors[count - 1].work.left + 40, monitors[count - 1].work.top
         + 40, 320, 240, false);
   before = rect;
   AdjustWindowRectEx(&rect, NATIVE_W32_STYLE, FALSE, 0);
   SetWindowPos(secondary, NULL, rect.left, rect.top, 0, 0,
         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
   native_w32_wait_rect(ctx, data, request, window, before);

   /* Borderless fullscreen where it is, then on a preferred monitor */
   request->settings.monitor    = 0;
   request->settings.fullscreen = true;
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window,
         monitors[count - 1].rect);
   CHECK(native_w32_fullscreen_style(secondary));
   request->settings.monitor    = 1;
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, monitors[0].rect);
   CHECK(native_w32_fullscreen_style(secondary));

   /* A window again with automatic bounds: where it stood before */
   request->settings.fullscreen = false;
   request->settings.monitor    = 0;
   request->settings.dims       = 0;
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, before);
   CHECK(!native_w32_fullscreen_style(secondary));

   /* Fullscreen again, then a window at a saved origin */
   request->settings.dims       = VIDEO_SCALE_PACK(320, 240);
   request->settings.fullscreen = true;
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window,
         monitors[count - 1].rect);
   request->settings.fullscreen = false;
   request->settings.pos        = VIDEO_POS_PACK(monitors[0].work.left + 50,
         monitors[0].work.top + 50);
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &monitors[0].work, monitors[0].work.left + 50,
         monitors[0].work.top + 50, 320, 240, false));
   CHECK(!native_w32_fullscreen_style(secondary));
   CHECK(native_w32_main_intact(&main_state));

   /* A request of an older generation changes nothing */
   native_window_settle(ctx, data, request, 100000);
   other                = *request;
   other.generation     = request->generation - 1;
   other.settings.pos   = VIDEO_POS_PACK(monitors[0].work.left + 200,
         monitors[0].work.top + 200);
   other.settings.dims  = VIDEO_SCALE_PACK(200, 150);
   ctx->windows->update(data, window, &other);
   seen = native_window_settle(ctx, data, request, 200000);
   CHECK(!(seen & NATIVE_EVENT(GEOMETRY)));
   CHECK(window->dims == VIDEO_SCALE_PACK(320, 240));

   /* Minimised and restored: skipped meanwhile, never closed. A move
    * asked for meanwhile is made once it is back. */
   ShowWindow(secondary, SW_MINIMIZE);
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(SUSPEND), 0);
   CHECK(!window->presentable);
   CHECK(window->dims == VIDEO_SCALE_PACK(320, 240));
   request->settings.pos = VIDEO_POS_PACK(monitors[0].work.left + 70,
         monitors[0].work.top + 70);
   ctx->windows->update(data, window, request);
   seen |= native_window_settle(ctx, data, request, 50000);
   CHECK(IsIconic(secondary) && !window->presentable);
   ShowWindow(secondary, SW_RESTORE);
   seen |= native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   CHECK(window->presentable);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &monitors[0].work, monitors[0].work.left + 70,
         monitors[0].work.top + 70, 320, 240, false));
   CHECK(native_w32_main_intact(&main_state));

   /* Maximised by the user, then placed: from its restored state. No
    * pump in between, where Wine without a window manager may restore
    * it itself. */
   ShowWindow(secondary, SW_MAXIMIZE);
   CHECK(IsZoomed(secondary));
   request->settings.pos = VIDEO_POS_PACK(monitors[0].work.left + 60,
         monitors[0].work.top + 60);
   ctx->windows->update(data, window, request);
   CHECK(!IsZoomed(secondary));
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &monitors[0].work, monitors[0].work.left + 60,
         monitors[0].work.top + 60, 320, 240, false));
   CHECK(window->presentable);

   native_gl_binding(ctx, data, window);
   CHECK(native_w32_main_intact(&main_state));

   /* Focus in and out, from the main window (a restore may have given
    * the window the focus) */
   SetFocus(main_window.hwnd);
   native_window_settle(ctx, data, request, 20000);
   CHECK(!window->focused);
   native_w32_focus(ctx, data, request, window, true);
   native_w32_focus(ctx, data, request, window, false);

   /* Close and focus loss outlast a flood the consumer has no room for */
   for (i = 0; i < 128; i++)
      PostMessage(secondary, WM_KILLFOCUS, 0, 0);
   PostMessage(secondary, WM_CLOSE, 0, 0);
   native_w32_pump(data);
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
   CHECK(IsWindow(secondary));
   CHECK(native_w32_main_intact(&main_state));
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));

   /* What is still queued for the closed window cannot reach the one
    * that replaces it */
   PostMessage(secondary, WM_SIZE, SIZE_RESTORED, MAKELPARAM(500, 400));
   PostMessage(secondary, WM_CLOSE, 0, 0);
   last = request->generation;
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL && window->surface == NULL);
   CHECK(!IsWindow(secondary));
   request->generation = video_driver_window_generation();
   CHECK(request->generation > last);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(window->generation == request->generation);
   seen = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 50000);
   CHECK(!(seen & NATIVE_EVENT(CLOSE)));
   CHECK(window->presentable);
   CHECK(window->dims == request->settings.dims);
   secondary = (HWND)window->surface;

   /* Closing the focused window takes its focus with it */
   native_w32_focus(ctx, data, request, window, true);
   PostMessage(secondary, WM_CLOSE, 0, 0);
   seen = native_window_drain(ctx, data, request);
   CHECK(seen & NATIVE_EVENT(CLOSE));
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->focused && !window->presentable);
   /* ... and the closed window reports nothing more */
   SetWindowPos(secondary, NULL, 0, 0, 200, 200,
         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
   SetFocus(secondary);
   SetFocus(main_window.hwnd);
   CHECK(!native_window_settle(ctx, data, request, 50000));
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = (HWND)window->surface;

   /* The system menu's close, which Alt+F4 sends too */
   PostMessage(secondary, WM_SYSCOMMAND, SC_CLOSE, 0);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(IsWindow(secondary));
   CHECK(native_w32_main_intact(&main_state));
   ctx->windows->destroy(data, window);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = (HWND)window->surface;

   /* Destroyed by something else: closed, and nothing is done to the
    * window again */
   DestroyWindow(secondary);
   request->settings.pos = VIDEO_POS_PACK(monitors[0].work.left + 80,
         monitors[0].work.top + 80);
   ctx->windows->update(data, window, request);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(!window->presentable);
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL);
   native_window_settle(ctx, data, request, 50000);
   CHECK(native_w32_main_intact(&main_state));

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
#ifdef HAVE_THREADS
   {
      /* Only the main window's thread pumps a window's messages */
      native_w32_elsewhere_t task;
      sthread_t *thread;
      memset(&task, 0, sizeof(task));
      task.ctx     = ctx;
      task.data    = data;
      task.request = *request;
      thread       = sthread_create(native_w32_create_elsewhere, &task);
      CHECK(thread != NULL);
      if (thread)
         sthread_join(thread);
      CHECK(!task.created && !task.window.state);
   }
#endif
   CHECK(!invalid.state && !invalid.surface && !invalid.display);
   CHECK(!native_window_settle(ctx, data, request, 200000));
   CHECK(native_w32_titled(request->screen) == 0);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   CHECK(native_w32_titled(request->screen) == 1);

   /* Made fullscreen on a chosen monitor; then a window with automatic
    * bounds: the main window's size, centred on its monitor */
   ctx->windows->destroy(data, window);
   request->generation          = video_driver_window_generation();
   request->settings.fullscreen = true;
   request->settings.monitor    = 1;
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   secondary = (HWND)window->surface;
   rect      = native_w32_client(secondary);
   CHECK(EqualRect(&rect, &monitors[0].rect));
   CHECK(native_w32_fullscreen_style(secondary));
   CHECK(window->dims == VIDEO_SCALE_PACK(rect.right - rect.left,
         rect.bottom - rect.top));
   GetClientRect(main_window.hwnd, &rect);
   request->settings.fullscreen = false;
   request->settings.monitor    = 0;
   request->settings.dims       = 0;
   ctx->windows->update(data, window, request);
   native_w32_wait_rect(ctx, data, request, window, native_w32_expect(
         &home->work, 0, 0, rect.right, rect.bottom, true));
   CHECK(!native_w32_fullscreen_style(secondary));
   request->settings.dims       = VIDEO_SCALE_PACK(320, 240);

   native_w32_sizemove(ctx, data, request, window);
   native_w32_sizemove_pumped(ctx, data, request, window);
   CHECK(native_w32_main_intact(&main_state));
#ifdef NATIVE_WGL
   if (strcmp(ctx->ident, "wgl") == 0)
   {
      native_wgl_failures(ctx, data, request, window);
      CHECK(native_w32_main_intact(&main_state));
   }
#endif
   native_w32_monitors_check(ctx, data, request);
}

/* The main window left up by the last lifetime goes without taking the
 * list another driver put up since; a cold start replaces it, and a
 * main window not kept takes its own down */
static void native_w32_teardown(const char *ident, enum gfx_ctx_api api,
      bool core)
{
   static video_window_display_t foreign[1];
   native_w32_monitor_t monitors[VIDEO_WINDOW_DISPLAYS_MAX];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   const gfx_ctx_driver_t *ctx;
   unsigned n, want;
   void *data = NULL;
   native_w32_kept = false;
   memset(foreign, 0, sizeof(foreign));
   foreign[0].ordinal = 1;
   foreign[0].valid   = foreign[0].on = foreign[0].physical = true;
   foreign[0].monitor = true;
   strlcpy(foreign[0].name, "foreign", sizeof(foreign[0].name));
   video_driver_publish_window_displays(foreign, 1);
   win32_window_release_kept();
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 1 && string_is_equal(got[0].name, "foreign"));
#ifdef HAVE_VULKAN
   if (strcmp(ident, "vk_w") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_w_vk, ident, api, 1, 0, false, &data);
   else
#endif
   if (strcmp(ident, "d3d") == 0)
      ctx = native_w32_d3d_init(&data);
   else
      ctx = video_context_driver_init_first(NULL, ident, api,
            core ? 3 : 0, core ? 2 : 0, false, &data);
   CHECK(ctx != NULL);
   if (!ctx)
      return;
   if (!ctx->set_video_mode(data, VIDEO_SCALE_PACK(640, 480), false))
   {
      CHECK(!"set_video_mode");
      return;
   }
   /* Before anything pumped */
   want = native_w32_monitors(monitors, VIDEO_WINDOW_DISPLAYS_MAX);
   n    = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   fprintf(stderr, "native win32: %u monitor(s) published at init%s\n",
         n, n && string_is_equal(got[0].name, "foreign")
         ? ", the foreign list" : "");
   CHECK(native_w32_monitors_match(monitors, want, got, n));
   native_w32_monitors_wait(ctx, data, NULL, monitors, want, got);
   /* Quitting: not kept */
   runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_SHUTDOWN_INITIATED;
   ctx->destroy(data);
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_SHUTDOWN_INITIATED;
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 0);
}
#endif

/* The Wayland contexts' native-window lifecycle for
 * video_screens_test.c, #included there. */
#ifdef HAVE_WAYLAND
#ifdef NATIVE_EGL
static void *native_wl_drawable(const video_window_native_t *window)
{
   return ((wl_video_window_t*)window->state)->drawable;
}
#endif

static bool native_wl_ident(const char *ident)
{
   return    strcmp(ident, "wayland") == 0
          || strcmp(ident, "vk_wayland") == 0;
}

/* The main window's state, which a secondary must leave alone */
typedef struct
{
   struct wl_surface *surface;
   struct xdg_toplevel *toplevel;
   void *frame;
   output_info_t *output;
   struct wp_viewport *viewport;
   void *win;
   void *egl_surface;
   unsigned dims;
   unsigned buffer_dims;
   unsigned buffer_scale;
   unsigned fractional_scale_num;
} native_wl_main_t;

static void native_wl_main_get(gfx_ctx_wayland_data_t *wl,
      native_wl_main_t *main_window)
{
   main_window->surface      = wl->surface;
   main_window->toplevel     = wl->xdg_toplevel;
   main_window->frame        = NULL;
#ifdef HAVE_LIBDECOR_H
   main_window->frame        = wl->libdecor_frame;
#endif
   main_window->output       = wl->current_output;
   main_window->viewport     = wl->viewport;
   main_window->win          = NULL;
   main_window->egl_surface  = NULL;
#ifdef HAVE_EGL
   main_window->win          = wl->win;
   main_window->egl_surface  = wl->egl.surf;
#endif
   main_window->dims         = wl->dims;
   main_window->buffer_dims  = wl->buffer_dims;
   main_window->buffer_scale = wl->buffer_scale;
   main_window->fractional_scale_num = wl->fractional_scale_num;
}

static bool native_wl_main_intact(gfx_ctx_wayland_data_t *wl,
      const native_wl_main_t *main_window)
{
   native_wl_main_t now;
   native_wl_main_get(wl, &now);
   if (     now.surface && now.surface == main_window->surface
         && now.toplevel     == main_window->toplevel
         && now.frame        == main_window->frame
         && now.output       == main_window->output
         && now.viewport     == main_window->viewport
         && now.win          == main_window->win
         && now.egl_surface  == main_window->egl_surface
         && now.dims         == main_window->dims
         && now.buffer_dims  == main_window->buffer_dims
         && now.buffer_scale == main_window->buffer_scale
         && now.fractional_scale_num == main_window->fractional_scale_num
         && !wl->fullscreen)
      return true;
   fprintf(stderr, "native wayland: main window changed: output %p (was %p),"
         " %ux%u (was %ux%u), scale %u/%u (was %u/%u), fullscreen %d\n",
         (void*)now.output, (void*)main_window->output,
         VIDEO_SCALE_W(now.dims), VIDEO_SCALE_H(now.dims),
         VIDEO_SCALE_W(main_window->dims), VIDEO_SCALE_H(main_window->dims),
         now.buffer_scale, now.fractional_scale_num,
         main_window->buffer_scale, main_window->fractional_scale_num,
         wl->fullscreen);
   return false;
}

/* Output 'monitor', counted as the fullscreen setting counts them */
static output_info_t *native_wl_output(gfx_ctx_wayland_data_t *wl,
      unsigned monitor)
{
   display_output_t *od;
   unsigned i = 0;
   wl_list_for_each(od, &wl->all_outputs, link)
   {
      if (++i == monitor)
         return od->output;
   }
   return NULL;
}

/* The first output the window is on */
static output_info_t *native_wl_output_of(gfx_ctx_wayland_data_t *wl,
      const wl_video_window_t *state)
{
   display_output_t *od;
   unsigned i;
   for (i = 0; i < WL_VIDEO_WINDOW_OUTPUTS; i++)
   {
      wl_list_for_each(od, &wl->all_outputs, link)
      {
         if (state->outputs[i] && od->output->output == state->outputs[i])
            return od->output;
      }
   }
   return NULL;
}

static bool native_wl_on(const wl_video_window_t *state,
      const output_info_t *oi)
{
   unsigned i;
   for (i = 0; oi && i < WL_VIDEO_WINDOW_OUTPUTS; i++)
      if (state->outputs[i] == oi->output)
         return true;
   return false;
}

/* The buffer for a surface of w x h at the window's scale */
static unsigned native_wl_buffer(const wl_video_window_t *state,
      unsigned w, unsigned h)
{
   if (state->fractional_scale)
      return VIDEO_SCALE_PACK(
            FRACTIONAL_SCALE_MULT(w, state->fractional_scale_num),
            FRACTIONAL_SCALE_MULT(h, state->fractional_scale_num));
   if (state->viewport)
      return VIDEO_SCALE_PACK(w * state->scale, h * state->scale);
   return VIDEO_SCALE_PACK(w, h);
}

/* dbus-send to KWin with 'args' (up to 6, NULL-terminated), no shell
 * between; the first line it prints goes to 'out' */
static bool native_wl_dbus(const char *const *args, char *out, size_t len)
{
   int fds[2];
   pid_t pid;
   ssize_t got;
   char buf[256];
   const char *argv[11];
   unsigned n      = 0;
   size_t have     = 0;
   int status      = 0;
   argv[n++]       = "dbus-send";
   argv[n++]       = "--session";
   argv[n++]       = "--print-reply=literal";
   argv[n++]       = "--dest=org.kde.KWin";
   while (*args && n < 10)
      argv[n++]    = *args++;
   argv[n]         = NULL;
   if (pipe(fds))
      return false;
   if (!(pid = fork()))
   {
      dup2(fds[1], 1);
      close(fds[0]);
      close(fds[1]);
      execvp(argv[0], (char *const*)argv);
      _exit(127);
   }
   close(fds[1]);
   while ((got = read(fds[0], buf, sizeof(buf))) > 0)
   {
      if (out && have + 1 < len)
      {
         size_t take = MIN((size_t)got, len - 1 - have);
         memcpy(out + have, buf, take);
         have += take;
      }
   }
   close(fds[0]);
   if (out && len)
      out[have] = '\0';
   if (pid < 0 || waitpid(pid, &status, 0) != pid)
      return false;
   return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* The scratch directory VIDEO_SCREENS_KWIN names, for the scripts of
 * the nested KWin the harness runs this under; NULL where there is none */
static const char *native_wl_kwin_dir(void)
{
   const char *dir = getenv("VIDEO_SCREENS_KWIN");
   return dir && *dir ? dir : NULL;
}

/* Runs 'body' as a script of the nested KWin the harness runs this
 * under, with 'w' this process's window captioned exactly for 'screen',
 * 'm' its main window and 'n' its windows captioned for any screen,
 * and returns once it has run (the script object's run replies
 * then; Scripting.start does not wait). VIDEO_SCREENS_KWIN names the
 * scratch directory the script goes in; without it there is no KWin to
 * ask, and this is false. */
static bool native_wl_kwin(unsigned screen, const char *body)
{
   static char path[1024];
   static char arg[1100];
   char reply[128];
   char object[64];
   const char *args[5];
   FILE *file;
   int id          = -1;
   const char *dir = native_wl_kwin_dir();
   if (!dir)
      return false;
   snprintf(path, sizeof(path), "%s/video_screens_kwin.js", dir);
   if (!(file = fopen(path, "w")))
   {
      CHECK(!"KWin script file");
      return true;
   }
   fprintf(file,
         "var w = null, m = null, n = 0, l = workspace.windowList();\n"
         "for (var i = 0; i < l.length; i++) {\n"
         "   if (l[i].pid != %d) continue;\n"
         "   if (l[i].caption.indexOf(\"" NATIVE_TITLE "\") == 0) n++;\n"
         "   if (l[i].caption == \"" NATIVE_TITLE "%u\") w = l[i];\n"
         "   else if (l[i].caption.indexOf(\"" NATIVE_TITLE "\") != 0)"
         " m = l[i];\n"
         "}\n"
         "if (w) { %s }\n", (int)getpid(), screen + 1, body);
   fclose(file);
   snprintf(arg, sizeof(arg), "string:%s", path);
   args[0] = "/Scripting";
   args[1] = "org.kde.kwin.Scripting.loadScript";
   args[2] = arg;
   args[3] = "string:video_screens_test";
   args[4] = NULL;
   if (native_wl_dbus(args, reply, sizeof(reply)))
      sscanf(reply, " int32 %d", &id);
   CHECK(id >= 0);
   if (id >= 0)
   {
      snprintf(object, sizeof(object), "/Scripting/Script%d", id);
      args[0] = object;
      args[1] = "org.kde.kwin.Script.run";
      args[2] = NULL;
      CHECK(native_wl_dbus(args, NULL, 0));
   }
   args[0] = "/Scripting";
   args[1] = "org.kde.kwin.Scripting.unloadScript";
   args[2] = "string:video_screens_test";
   args[3] = NULL;
   native_wl_dbus(args, NULL, 0);
   remove(path);
   return true;
}

/* Something on the surface, so the compositor maps the window: a frame
 * where the context draws, one pixel where it does not (Vulkan) */
static struct wl_buffer *native_wl_pixel;

static void native_wl_map(const gfx_ctx_driver_t *ctx, void *data,
      video_window_native_t *window)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (ctx->windows->bind_gl)
   {
      CHECK(ctx->windows->bind_gl(data, window));
      CHECK(ctx->windows->present_gl(data, window));
      CHECK(ctx->windows->bind_gl(data, NULL));
      return;
   }
   if (native_wl_pixel)
      wl_buffer_destroy(native_wl_pixel);
   native_wl_pixel = NULL;
   if (!wl->single_pixel_manager)
      return;
   native_wl_pixel = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
         wl->single_pixel_manager, 0, 0, 0, UINT32_MAX);
   wl_surface_attach((struct wl_surface*)window->surface,
         native_wl_pixel, 0, 0);
   wl_surface_commit((struct wl_surface*)window->surface);
   wl_display_flush(wl->input.dpy);
}

#ifdef HAVE_THREADS
static sthread_t *native_wl_thread;
static retro_atomic_int_t native_wl_running;

/* The frontend's thread: the input driver's poll */
static void native_wl_input_loop(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   while (retro_atomic_load_acquire_int(&native_wl_running))
   {
      wayland_input_dispatch(&wl->input);
      retro_sleep(1);
   }
}
#else
static void native_wl_input_inline(void *data)
{
   wayland_input_dispatch(&((gfx_ctx_wayland_data_t*)data)->input);
}
#endif

/* The input driver's poll, on a thread of its own as the frontend's,
 * where there are threads */
static void native_wl_input(gfx_ctx_wayland_data_t *wl, bool on)
{
#ifdef HAVE_THREADS
   if (on && !native_wl_thread)
   {
      retro_atomic_store_release_int(&native_wl_running, 1);
      native_wl_thread = sthread_create(native_wl_input_loop, wl);
      CHECK(native_wl_thread != NULL);
   }
   else if (!on && native_wl_thread)
   {
      retro_atomic_store_release_int(&native_wl_running, 0);
      sthread_join(native_wl_thread);
      native_wl_thread = NULL;
   }
#else
   native_window_input = on ? native_wl_input_inline : NULL;
#endif
}

/* RetroArch's keyboard focus as the seat's handlers keep it, read
 * while the frontend's thread is not writing it */
static bool native_wl_keyboard_focus(gfx_ctx_wayland_data_t *wl)
{
   bool focus;
   native_wl_input(wl, false);
   focus = wl->input.keyboard_focus;
   native_wl_input(wl, true);
   return focus;
}

typedef struct
{
   gfx_ctx_wayland_data_t *wl;
   struct wl_surface *surface;
   unsigned enters;
   bool leave;
   bool enter;
} native_wl_flood_t;

/* Keyboard enters for the surface, then a leave and an enter where
 * asked for, handed to the seat's handlers as libwayland would */
static void native_wl_flood_run(void *data)
{
   unsigned i;
   struct wl_array keys;
   native_wl_flood_t *flood = (native_wl_flood_t*)data;
   wl_array_init(&keys);
   for (i = 0; i < flood->enters; i++)
      keyboard_listener.enter(flood->wl, flood->wl->wl_keyboard, i,
            flood->surface, &keys);
   if (flood->leave)
      keyboard_listener.leave(flood->wl, flood->wl->wl_keyboard, i,
            flood->surface);
   if (flood->enter)
      keyboard_listener.enter(flood->wl, flood->wl->wl_keyboard, i,
            flood->surface, &keys);
   wl_array_release(&keys);
}

/* On the frontend's thread where there is one, else on this one; the
 * real keyboard focus is put back afterwards */
static void native_wl_flood(gfx_ctx_wayland_data_t *wl,
      struct wl_surface *surface, unsigned enters, bool leave, bool enter,
      bool frontend)
{
   native_wl_flood_t flood;
   bool focus;
   flood.wl      = wl;
   flood.surface = surface;
   flood.enters  = enters;
   flood.leave   = leave;
   flood.enter   = enter;
   native_wl_input(wl, false);
   focus         = wl->input.keyboard_focus;
#ifdef HAVE_THREADS
   if (frontend)
   {
      sthread_t *thread = sthread_create(native_wl_flood_run, &flood);
      CHECK(thread != NULL);
      if (thread)
         sthread_join(thread);
   }
   else
#endif
      native_wl_flood_run(&flood);
   wl->input.keyboard_focus = focus;
   native_wl_input(wl, true);
}

/* The pointer's place, its wheel and touches are the main window's
 * alone: over the screen's window they leave the main window's as
 * they were, while relative motion is still taken. Handed to the seat's
 * handlers as libwayland would, the frontend's thread stopped; the
 * seat's state is put back after. */
static void native_wl_pointer(gfx_ctx_wayland_data_t *wl,
      struct wl_surface *surface)
{
   uint32_t pos;
   input_ctx_wayland_data_t input;
   struct wl_surface *other;
   int touches;
   wl_fixed_t x = wl_fixed_from_int(70);
   wl_fixed_t y = wl_fixed_from_int(90);
   native_wl_input(wl, false);
   input   = wl->input;
   touches = wl->num_active_touches;

   pointer_listener.enter(wl, wl->wl_pointer, 1, wl->surface,
         wl_fixed_from_int(10), wl_fixed_from_int(20));
   pos = wl->input.mouse.pos;
   pointer_listener.motion(wl, wl->wl_pointer, 0,
         wl_fixed_from_int(30), wl_fixed_from_int(40));
   CHECK(wl->input.mouse.pos != pos);
   pos = wl->input.mouse.pos;
   pointer_listener.leave(wl, wl->wl_pointer, 2, wl->surface);

   pointer_listener.enter(wl, wl->wl_pointer, 3, surface,
         wl_fixed_from_int(50), wl_fixed_from_int(60));
   CHECK(wl->input.mouse.pos == pos);
   pointer_listener.motion(wl, wl->wl_pointer, 0, x, y);
   CHECK(wl->input.mouse.pos == pos);
   wl->input.mouse.delta = 0;
   relative_pointer_listener.relative_motion(wl, NULL, 0, 0,
         wl_fixed_from_int(5), wl_fixed_from_int(6),
         wl_fixed_from_int(5), wl_fixed_from_int(6));
   CHECK(wl->input.mouse.delta == VIDEO_POS_PACK(5, 6));
#ifdef WL_POINTER_FRAME_SINCE_VERSION
   pointer_listener.axis_discrete(wl, wl->wl_pointer,
         WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
   pointer_listener.frame(wl, wl->wl_pointer);
   CHECK(!wl->input.mouse.wu && !wl->input.mouse.wd);
#endif
   touch_listener.down(wl, NULL, 4, 0, surface, 7, x, y);
   CHECK(wl->num_active_touches == touches);
   touch_listener.up(wl, NULL, 5, 0, 7);
   pointer_listener.leave(wl, wl->wl_pointer, 6, surface);

   /* Nor over another of the client's surfaces, as a libdecor title
    * bar is, or one libwayland no longer knows */
   other = wl_compositor_create_surface(wl->compositor);
   pointer_listener.enter(wl, wl->wl_pointer, 11, other, x, y);
   CHECK(wl->input.mouse.pos == pos);
   pointer_listener.motion(wl, wl->wl_pointer, 0, x, y);
   CHECK(wl->input.mouse.pos == pos);
#ifdef WL_POINTER_FRAME_SINCE_VERSION
   pointer_listener.axis_discrete(wl, wl->wl_pointer,
         WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
   pointer_listener.frame(wl, wl->wl_pointer);
   CHECK(!wl->input.mouse.wu && !wl->input.mouse.wd);
#endif
   touch_listener.down(wl, NULL, 12, 0, other, 7, x, y);
   CHECK(wl->num_active_touches == touches);
   touch_listener.up(wl, NULL, 13, 0, 7);
   pointer_listener.leave(wl, wl->wl_pointer, 14, other);
   pointer_listener.enter(wl, wl->wl_pointer, 15, NULL, x, y);
   CHECK(wl->input.mouse.pos == pos);
   pointer_listener.leave(wl, wl->wl_pointer, 16, NULL);

   /* Nor a click over the screen's window once the main window's surface
    * has entered an output */
   pointer_listener.enter(wl, wl->wl_pointer, 17, surface, x, y);
   wl_surface_listener.enter(wl, wl->surface, NULL);
   pointer_listener.button(wl, wl->wl_pointer, 18, 0, BTN_LEFT,
         WL_POINTER_BUTTON_STATE_PRESSED);
   CHECK(!wl->input.mouse.left);
   pointer_listener.button(wl, wl->wl_pointer, 19, 0, BTN_LEFT,
         WL_POINTER_BUTTON_STATE_RELEASED);
   pointer_listener.leave(wl, wl->wl_pointer, 20, surface);

   /* The same over the main window is taken */
   pointer_listener.enter(wl, wl->wl_pointer, 7, wl->surface, x, y);
   CHECK(wl->input.mouse.pos != pos);
#ifdef WL_POINTER_FRAME_SINCE_VERSION
   pointer_listener.axis_discrete(wl, wl->wl_pointer,
         WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
   pointer_listener.frame(wl, wl->wl_pointer);
   CHECK(wl->input.mouse.wu);
#endif
   touch_listener.down(wl, NULL, 8, 0, wl->surface, 7, x, y);
   CHECK(wl->num_active_touches == touches + 1);
   touch_listener.up(wl, NULL, 9, 0, 7);
   CHECK(wl->num_active_touches == touches);
   pointer_listener.leave(wl, wl->wl_pointer, 10, wl->surface);
   wl_surface_destroy(other);

   wl->input = input;
   native_wl_input(wl, true);
}

/* The last screen's window destroyed with the pointer over it: motion
 * and wheel still on their way for it are not the main window's */
static void native_wl_pointer_gone(const gfx_ctx_driver_t *ctx, void *data,
      gfx_ctx_wayland_data_t *wl, video_window_native_t *window)
{
   uint32_t pos;
   input_ctx_wayland_data_t input;
   native_wl_input(wl, false);
   input = wl->input;
   pointer_listener.enter(wl, wl->wl_pointer, 1, wl->surface,
         wl_fixed_from_int(10), wl_fixed_from_int(20));
   pos   = wl->input.mouse.pos;
   pointer_listener.leave(wl, wl->wl_pointer, 2, wl->surface);
   pointer_listener.enter(wl, wl->wl_pointer, 3,
         (struct wl_surface*)window->surface,
         wl_fixed_from_int(50), wl_fixed_from_int(60));
   ctx->windows->destroy(data, window);
   pointer_listener.motion(wl, wl->wl_pointer, 0,
         wl_fixed_from_int(70), wl_fixed_from_int(90));
   CHECK(wl->input.mouse.pos == pos);
#ifdef WL_POINTER_FRAME_SINCE_VERSION
   pointer_listener.axis_discrete(wl, wl->wl_pointer,
         WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
   pointer_listener.frame(wl, wl->wl_pointer);
   CHECK(!wl->input.mouse.wu && !wl->input.mouse.wd);
#endif
   pointer_listener.leave(wl, wl->wl_pointer, 4, NULL);
   wl->input = input;
   native_wl_input(wl, true);
}

/* With no screen's window open, another surface is taken as before,
 * but not one libwayland no longer knows */
static void native_wl_pointer_alone(gfx_ctx_wayland_data_t *wl)
{
   uint32_t pos;
   input_ctx_wayland_data_t input;
   struct wl_surface *other = wl_compositor_create_surface(wl->compositor);
   native_wl_input(wl, false);
   input = wl->input;
   pointer_listener.enter(wl, wl->wl_pointer, 1, wl->surface,
         wl_fixed_from_int(10), wl_fixed_from_int(20));
   pos   = wl->input.mouse.pos;
   pointer_listener.leave(wl, wl->wl_pointer, 2, wl->surface);
   pointer_listener.enter(wl, wl->wl_pointer, 3, other,
         wl_fixed_from_int(70), wl_fixed_from_int(90));
   CHECK(wl->input.mouse.pos != pos);
   pos   = wl->input.mouse.pos;
   pointer_listener.leave(wl, wl->wl_pointer, 4, other);
   pointer_listener.enter(wl, wl->wl_pointer, 5, NULL,
         wl_fixed_from_int(30), wl_fixed_from_int(40));
   CHECK(wl->input.mouse.pos == pos);
   pointer_listener.motion(wl, wl->wl_pointer, 0,
         wl_fixed_from_int(50), wl_fixed_from_int(60));
   CHECK(wl->input.mouse.pos == pos);
   pointer_listener.leave(wl, wl->wl_pointer, 6, NULL);
   wl_surface_destroy(other);
   wl->input = input;
   native_wl_input(wl, true);
}

/* Runs 'script' (KWin's, NULL for none) and pumps until the window's
 * keyboard focus is 'focused' and the event saying so arrived */
static void native_wl_wait_focus(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, bool focused, const char *script)
{
   unsigned seen         = 0;
   unsigned wanted       = focused ? NATIVE_EVENT(FOCUS_GAIN)
      : NATIVE_EVENT(FOCUS_LOSS);
   retro_time_t deadline;
   if (script)
      native_wl_kwin(video_window_title_screen(request), script);
   deadline = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      seen |= native_window_drain(ctx, data, request);
      if ((seen & wanted) && window->focused == focused)
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: focus %d, saw 0x%x\n",
         window->focused, seen);
   CHECK(!"keyboard focus");
}

/* Fullscreen with 'monitor' preferred: on 'oi' and filling it */
static void native_wl_fullscreen(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      unsigned monitor, const output_info_t *oi)
{
   wl_video_window_t *state = (wl_video_window_t*)window->state;
   retro_time_t deadline    = cpu_features_get_time_usec() + 3000000;
   request->settings.monitor    = monitor;
   request->settings.fullscreen = true;
   ctx->windows->update(data, window, request);
   for (;;)
   {
      native_window_drain(ctx, data, request);
      /* A fractional scale can round the buffer a pixel off the mode */
      if (     oi && state->fullscreen && native_wl_on(state, oi)
            && VIDEO_SCALE_W(window->dims) + 1 >= VIDEO_SCALE_W(oi->dims)
            && VIDEO_SCALE_W(window->dims) <= VIDEO_SCALE_W(oi->dims) + 1
            && VIDEO_SCALE_H(window->dims) + 1 >= VIDEO_SCALE_H(oi->dims)
            && VIDEO_SCALE_H(window->dims) <= VIDEO_SCALE_H(oi->dims) + 1)
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: fullscreen %d on %d, %ux%u, output %ux%u\n",
         state->fullscreen, native_wl_on(state, oi),
         VIDEO_SCALE_W(window->dims), VIDEO_SCALE_H(window->dims),
         oi ? VIDEO_SCALE_W(oi->dims) : 0, oi ? VIDEO_SCALE_H(oi->dims) : 0);
   CHECK(!"fullscreen output");
}

/* A window again, at the size it had */
static void native_wl_windowed(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   wl_video_window_t *state = (wl_video_window_t*)window->state;
   retro_time_t deadline    = cpu_features_get_time_usec() + 3000000;
   request->settings.monitor    = 0;
   request->settings.fullscreen = false;
   ctx->windows->update(data, window, request);
   for (;;)
   {
      native_window_drain(ctx, data, request);
      if (     !state->fullscreen
            && state->dims == request->settings.dims
            && window->dims == native_wl_buffer(state,
               VIDEO_SCALE_W(state->dims), VIDEO_SCALE_H(state->dims)))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: fullscreen %d, %ux%u\n",
         state->fullscreen, VIDEO_SCALE_W(state->dims),
         VIDEO_SCALE_H(state->dims));
   CHECK(!"windowed");
}

/* The compositor's close: KWin's, else as libwayland would hand it to
 * the toplevel's listener */
static void native_wl_close(gfx_ctx_wayland_data_t *wl,
      wl_video_window_t *state)
{
   const struct xdg_toplevel_listener *listener;
   struct xdg_toplevel *toplevel = state->xdg_toplevel;
   if (native_wl_kwin(video_window_title_screen(&state->common.request),
            "w.closeWindow();"))
      return;
#ifdef HAVE_LIBDECOR_H
   if (state->libdecor_frame)
      toplevel = wl->libdecor_frame_get_xdg_toplevel(state->libdecor_frame);
#endif
   listener = (const struct xdg_toplevel_listener*)wl_proxy_get_listener(
         (struct wl_proxy*)toplevel);
   CHECK(listener && listener->close);
   if (listener && listener->close)
      listener->close(wl_proxy_get_user_data((struct wl_proxy*)toplevel),
            toplevel);
}

/* Takes the keyboard focus to the window: KWin's, else the seat's
 * enter handed to its handler on this thread */
static void native_wl_focus(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, video_window_native_t *window)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (!native_wl_kwin_dir())
   {
      native_wl_flood(wl, (struct wl_surface*)window->surface, 1, false,
            false, false);
      native_wl_wait_focus(ctx, data, request, window, true, NULL);
      return;
   }
   native_wl_wait_focus(ctx, data, request, window, true,
         "workspace.activeWindow = w;");
}

/* A preferred scale for the window, handed to its listener as
 * libwayland would: the fractional one where it is bound */
static void native_wl_rescale(wl_video_window_t *state, unsigned scale)
{
   if (state->fractional_scale)
   {
      const struct wp_fractional_scale_v1_listener *listener =
         (const struct wp_fractional_scale_v1_listener*)
         wl_proxy_get_listener((struct wl_proxy*)state->fractional_scale);
      listener->preferred_scale(state, state->fractional_scale,
            scale * FRACTIONAL_SCALE_V1_DEN);
   }
#ifdef WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
   else
   {
      const struct wl_surface_listener *listener =
         (const struct wl_surface_listener*)
         wl_proxy_get_listener((struct wl_proxy*)state->surface);
      listener->preferred_buffer_scale(state, state->surface,
            (int32_t)scale);
   }
#endif
}

/* Closes the window and, in the same drain, has its focus go with it */
static void native_wl_close_focused(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      video_window_native_t *window)
{
   unsigned seen;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   native_wl_close((gfx_ctx_wayland_data_t*)data,
         (wl_video_window_t*)window->state);
   for (;;)
   {
      seen = native_window_drain(ctx, data, request);
      if (     (seen & NATIVE_EVENT(CLOSE))
            || cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   CHECK(seen & NATIVE_EVENT(CLOSE));
   CHECK(native_window_closed_user);
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->focused && !window->presentable);
   /* Nor does a closed window report a new scale */
   native_wl_rescale((wl_video_window_t*)window->state, 3);
   CHECK(!(native_window_settle(ctx, data, request, 50000)
            & NATIVE_EVENT(GEOMETRY)));
}

static unsigned native_wl_outputs(gfx_ctx_wayland_data_t *wl)
{
   unsigned count = (unsigned)wl_list_length(&wl->all_outputs);
   return count < VIDEO_WINDOW_DISPLAYS_MAX
      ? count : VIDEO_WINDOW_DISPLAYS_MAX;
}

/* The published list is the outputs in the fullscreen setting's order,
 * the main window's marked */
static bool native_wl_monitors_match(gfx_ctx_wayland_data_t *wl,
      const video_window_display_t *got, unsigned n)
{
   unsigned i;
   if (n != native_wl_outputs(wl))
      return false;
   for (i = 0; i < n; i++)
   {
      output_info_t *oi = native_wl_output(wl, i + 1);
      if (     !oi || got[i].ordinal != i + 1 || got[i].dims != oi->dims
            || got[i].own != (oi == wl->current_output))
         return false;
   }
   return true;
}

/* Reads the list as the app thread does, pumping until it matches */
static unsigned native_wl_monitors_wait(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request,
      video_window_display_t *got)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   retro_time_t deadline      = cpu_features_get_time_usec() + 3000000;
   unsigned n;
   for (;;)
   {
      n = video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
      if (native_wl_monitors_match(wl, got, n))
         return n;
      if (cpu_features_get_time_usec() > deadline)
         break;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   fprintf(stderr, "native wayland: %u monitor(s) published for %u"
         " output(s)\n", n, native_wl_outputs(wl));
   CHECK(!"published monitors");
   return n;
}

/* The outputs as the screens' Display setting lists them, republished
 * when one changes or the main window goes to another, and only then */
static void native_wl_monitors(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request)
{
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   char make[64], model[64], cut[VIDEO_WINDOW_DISPLAY_MODEL_SIZE + 1];
   char published[VIDEO_WINDOW_DISPLAY_MODEL_SIZE];
   unsigned i, restore, retry, dims;
   gfx_ctx_wayland_data_t *wl     = (gfx_ctx_wayland_data_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();
   output_info_t *first           = native_wl_output(wl, 1);
   output_info_t *was             = wl->current_output;
   unsigned own                   = 0;
   unsigned n                     = native_wl_monitors_wait(ctx, data,
         request, got);
   CHECK(n >= 1 && first);
   if (!n || !first)
      return;
   for (i = 0; i < n; i++)
   {
      char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      output_info_t *oi = native_wl_output(wl, i + 1);
      CHECK(got[i].valid && got[i].on && got[i].physical && got[i].monitor);
      /* wl_output v4 names it; below that it keys by its ordinal */
      CHECK(oi && (wl_output_get_version(oi->output) < 4 || got[i].name[0]));
      if (got[i].own)
         own++;
      video_windows_display_key(got, i, key, sizeof(key));
      fprintf(stderr, "native wayland: monitor %u \"%s\"%s\n",
            got[i].ordinal, key, got[i].own ? ", main window" : "");
      CHECK(key[0] != '\0');
   }
   /* Enter comes after the main window's first commit */
   fprintf(stderr, "native wayland: main window %s an output\n",
         was ? "on" : "not yet on");
   CHECK(own == (was ? 1u : 0u));

   /* An output sent again unchanged publishes nothing */
   restore = video_st->windows_restore;
   retry   = video_st->windows_retry;
   output_listener.done(first, first->output);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(video_st->windows_retry == retry);

   /* A new mode does, and so does going back */
   dims = first->dims;
   output_listener.mode(first, first->output, WL_OUTPUT_MODE_CURRENT,
         VIDEO_SCALE_W(dims) + 16, VIDEO_SCALE_H(dims), first->refresh_rate);
   output_listener.done(first, first->output);
   native_wl_monitors_wait(ctx, data, request, got);
   CHECK(got[0].dims == VIDEO_SCALE_PACK(VIDEO_SCALE_W(dims) + 16,
            VIDEO_SCALE_H(dims)));
   CHECK(video_st->windows_retry != retry);
   CHECK(video_st->windows_restore == restore);
   output_listener.mode(first, first->output, WL_OUTPUT_MODE_CURRENT,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), first->refresh_rate);
   output_listener.done(first, first->output);
   native_wl_monitors_wait(ctx, data, request, got);

   /* A model too long for the menu is cut between characters: an e
    * acute across the end is dropped, not split */
   strlcpy(published, got[0].model, sizeof(published));
   strlcpy(make, first->make ? first->make : "", sizeof(make));
   strlcpy(model, first->model ? first->model : "", sizeof(model));
   memset(cut, 'M', sizeof(cut) - 3);
   memcpy(cut + sizeof(cut) - 3, "\xc3\xa9", 3);
   output_listener.geometry(first, first->output, 0, 0, 0, 0, 0, make, cut,
         0);
   output_listener.done(first, first->output);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   fprintf(stderr, "native wayland: a %u-byte model published as %u\n",
         (unsigned)strlen(cut), (unsigned)strlen(got[0].model));
   CHECK(strlen(got[0].model) == sizeof(cut) - 3);
   output_listener.geometry(first, first->output, 0, 0, 0, 0, 0, make,
         model, 0);
   output_listener.done(first, first->output);
   native_window_settle(ctx, data, request, 50000);
   video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(string_is_equal(got[0].model, published));

   /* The main window's own output follows it */
   if (was && n >= 2)
   {
      output_info_t *other = native_wl_output(wl, was == first ? 2 : 1);
      wl_surface_listener.enter(wl, wl->surface, other->output);
      CHECK(wl->current_output == other);
      native_wl_monitors_wait(ctx, data, request, got);
      wl_surface_listener.leave(wl, wl->surface, other->output);
      CHECK(wl->current_output == was);
      native_wl_monitors_wait(ctx, data, request, got);
   }
   else
      fprintf(stderr, "native wayland: the main window's output going"
            " to another not checked\n");
}

/* Pumps until a GEOMETRY with bounds to save arrives; false at the
 * deadline */
static bool native_wl_wait_save(const gfx_ctx_driver_t *ctx, void *data,
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
static void native_wl_no_save(const char *what, unsigned *saves)
{
   if (native_window_saves != *saves)
      fprintf(stderr, "native wayland: %s saved %u, the last %ux%u\n",
            what, native_window_saves - *saves,
            VIDEO_SCALE_W(native_window_saved.save_dims),
            VIDEO_SCALE_H(native_window_saved.save_dims));
   CHECK(native_window_saves == *saves);
   *saves = native_window_saves;
}

static void native_wl_replace(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   ctx->windows->destroy(data, window);
   CHECK(window->state == NULL && window->surface == NULL);
   request->generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, request, window));
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
   native_wl_map(ctx, data, window);
}

/* Maximised or back, by KWin or as the window asks, and pumped until
 * the compositor has done it */
static void native_wl_maximise(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request, video_window_native_t *window,
      bool on, bool by_kwin)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl_video_window_t *state   = (wl_video_window_t*)window->state;
   retro_time_t deadline      = cpu_features_get_time_usec() + 3000000;
   if (by_kwin)
      native_wl_kwin(video_window_title_screen(request), on
            ? "w.setMaximize(true, true);" : "w.setMaximize(false, false);");
#ifdef HAVE_LIBDECOR_H
   else if (state->libdecor_frame)
   {
      if (on)
         wl->libdecor_frame_set_maximized(state->libdecor_frame);
      else
         wl->libdecor_frame_unset_maximized(state->libdecor_frame);
   }
#endif
   else if (on)
      xdg_toplevel_set_maximized(state->xdg_toplevel);
   else
      xdg_toplevel_unset_maximized(state->xdg_toplevel);
   wl_display_flush(wl->input.dpy);
   while (state->floating == on)
   {
      if (cpu_features_get_time_usec() > deadline)
         break;
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   if (state->floating == on)
      fprintf(stderr, "native wayland: %s by %s not done\n",
            on ? "maximised" : "restored", by_kwin ? "KWin" : "the window");
   CHECK(state->floating != on);
}

/* Resized by the user, through KWin */
static void native_wl_kwin_size(unsigned screen, unsigned w, unsigned h)
{
   char body[192];
   snprintf(body, sizeof(body),
         "var g = w.frameGeometry, c = w.clientGeometry;"
         " w.frameGeometry = {x: g.x, y: g.y,"
         " width: %u + g.width - c.width,"
         " height: %u + g.height - c.height};", w, h);
   native_wl_kwin(screen, body);
}

/* Pumps until the compositor has it fullscreen or not and, with 'dims',
 * at that size */
static void native_wl_wait_state(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, bool fullscreen, unsigned dims)
{
   wl_video_window_t *state = (wl_video_window_t*)window->state;
   retro_time_t deadline    = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      native_window_drain(ctx, data, request);
      if (     state->fullscreen == fullscreen
            && (!dims || (state->dims == dims
                  && window->dims == native_wl_buffer(state,
                     VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims)))))
         return;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native wayland: fullscreen %d at %ux%u, wanted %d at "
         "%ux%u\n", state->fullscreen, VIDEO_SCALE_W(state->dims),
         VIDEO_SCALE_H(state->dims), fullscreen, VIDEO_SCALE_W(dims),
         VIDEO_SCALE_H(dims));
   CHECK(!"window state");
}

/* Pumps, with nothing polled, until a resize's size waits to go out */
static bool native_wl_wait_judged(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_native_t *window, unsigned dims)
{
   wl_video_window_t *state = (wl_video_window_t*)window->state;
   retro_time_t deadline    = cpu_features_get_time_usec() + 3000000;
   for (;;)
   {
      bool quit     = false;
      bool resize   = false;
      unsigned size = VIDEO_SCALE_PACK(640, 480);
      if (native_window_input)
         native_window_input(data);
      ctx->check_window(data, &quit, &resize, &size);
      if (state->common.save_dims == dims)
         return true;
      if (cpu_features_get_time_usec() > deadline)
         return false;
      retro_sleep(2);
   }
}

/* Fullscreen on and off through update(): back at 'dims', and none of
 * it a resize of the user's */
static void native_wl_toggle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *r, video_window_native_t *window,
      unsigned dims)
{
   unsigned saves = native_window_saves;
   r->settings.fullscreen = true;
   ctx->windows->update(data, window, r);
   native_wl_wait_state(ctx, data, r, window, true, 0);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("fullscreen", &saves);
   r->settings.fullscreen = false;
   ctx->windows->update(data, window, r);
   native_wl_wait_state(ctx, data, r, window, false, dims);
   native_window_settle(ctx, data, r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("a window again", &saves);
   native_wl_wait_state(ctx, data, r, window, false, dims);
}

/* An edit as the renderers hand it on. The compositor has the position,
 * what update() sizes a window to is not the user's, and the same
 * request again does nothing. */
static void native_wl_updates(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      bool kwin)
{
   static bool done;
   video_window_request_t r;
   wl_video_window_t *state;
   retro_time_t settle;
   void *surface;
   unsigned dims, saves, screen;

   if (done)
      return;
   done                  = true;
   r                     = *request;
   r.settings.monitor    = 0;
   r.settings.fullscreen = false;
   r.settings.pos        = 0;
   r.settings.pos_set    = false;
   r.settings.dims       = VIDEO_SCALE_PACK(360, 270);
   screen                = video_window_title_screen(&r);
   native_wl_replace(ctx, data, &r, window);
   state = (wl_video_window_t*)window->state;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   saves = native_window_saves;

   /* Resized by the user, so the next requests carry its saved size */
   if (kwin)
   {
      native_wl_kwin_size(screen, 400, 300);
      CHECK(native_wl_wait_save(ctx, data, &r));
      CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(400, 300));
      r.settings.dims = native_window_saved.save_dims;
      native_wl_wait_state(ctx, data, &r, window, false, r.settings.dims);
      native_window_settle(ctx, data, &r, 200000);
      saves = native_window_saves;
   }
   native_wl_toggle(ctx, data, &r, window, r.settings.dims);
   saves = native_window_saves;

   /* The X and Y rows: the compositor's, so nothing happens */
   dims              = state->dims;
   settle            = state->common.settle;
   r.settings.pos    = VIDEO_POS_PACK(300, 200);
   r.settings.pos_set = true;
   ctx->windows->update(data, window, &r);
   CHECK(!(native_window_settle(ctx, data, &r, 200000)
         & NATIVE_EVENT(GEOMETRY)));
   CHECK(state->common.settle == settle);
   CHECK(state->dims == dims);
   native_wl_no_save("the X and Y rows", &saves);

   /* The Size row */
   r.settings.dims = VIDEO_SCALE_PACK(420, 310);
   ctx->windows->update(data, window, &r);
   native_wl_wait_state(ctx, data, &r, window, false, r.settings.dims);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("the Size row", &saves);
   CHECK(state->dims == r.settings.dims);

   /* The same request, twice: nothing at all */
   settle = state->common.settle;
   ctx->windows->update(data, window, &r);
   ctx->windows->update(data, window, &r);
   CHECK(state->common.settle == settle);
   CHECK(!(native_window_settle(ctx, data, &r, 200000)
         & NATIVE_EVENT(GEOMETRY)));
   native_wl_no_save("the same request", &saves);

   /* A size asked for while the compositor sizes the window: it comes
    * with the next floating configure, however late */
   native_wl_maximise(ctx, data, &r, window, true, false);
   native_window_settle(ctx, data, &r, 100000);
   r.settings.dims = VIDEO_SCALE_PACK(430, 320);
   ctx->windows->update(data, window, &r);
   CHECK(state->floating_asked);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("a size while maximised", &saves);
   native_wl_maximise(ctx, data, &r, window, false, false);
   native_wl_wait_state(ctx, data, &r, window, false, r.settings.dims);
   native_window_settle(ctx, data, &r, 200000);
   native_wl_no_save("restored to a size asked for", &saves);

   r.settings.fullscreen = true;
   ctx->windows->update(data, window, &r);
   native_wl_wait_state(ctx, data, &r, window, true, 0);
   r.settings.dims = VIDEO_SCALE_PACK(440, 330);
   ctx->windows->update(data, window, &r);
   CHECK(state->floating_asked);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   r.settings.fullscreen = false;
   ctx->windows->update(data, window, &r);
   native_wl_wait_state(ctx, data, &r, window, false, r.settings.dims);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("a size while fullscreen", &saves);

   if (kwin)
   {
      /* A resize not yet out when an edit comes: the edit stands */
      native_wl_kwin_size(screen, 460, 340);
      CHECK(native_wl_wait_judged(ctx, data, window,
               VIDEO_SCALE_PACK(460, 340)));
      r.settings.dims = VIDEO_SCALE_PACK(410, 305);
      ctx->windows->update(data, window, &r);
      native_wl_wait_state(ctx, data, &r, window, false, r.settings.dims);
      native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
      native_wl_no_save("a resize before an edit", &saves);
      CHECK(state->common.request.settings.dims == r.settings.dims);

      /* Its own size back is no edit: that resize still goes out */
      native_wl_kwin_size(screen, 470, 350);
      CHECK(native_wl_wait_judged(ctx, data, window,
               VIDEO_SCALE_PACK(470, 350)));
      r.settings.dims = VIDEO_SCALE_PACK(470, 350);
      settle          = state->common.settle;
      ctx->windows->update(data, window, &r);
      CHECK(state->common.settle == settle);
      native_window_settle(ctx, data, &r, 200000);
      CHECK(native_window_saves == saves + 1);
      CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(470, 350));
      native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
      saves = native_window_saves;
   }

   /* An Auto window's first X or Y row, windowed: to no monitor, and
    * nothing shows */
   r.settings.monitor = 1;
   r.settings.pos     = 0;
   r.settings.pos_set = false;
   r.settings.dims    = VIDEO_SCALE_PACK(400, 300);
   native_wl_replace(ctx, data, &r, window);
   state = (wl_video_window_t*)window->state;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("made Auto", &saves);
   surface            = window->surface;
   r.settings.monitor = 0;
   r.settings.pos     = VIDEO_POS_PACK(300, 200);
   r.settings.pos_set = true;
   ctx->windows->update(data, window, &r);
   CHECK(!(native_window_settle(ctx, data, &r,
               VIDEO_WINDOW_SETTLE_USEC + 100000)
         & NATIVE_EVENT(GEOMETRY)));
   CHECK(window->surface == surface);
   CHECK(!state->fullscreen && state->floating);
   CHECK(state->dims == r.settings.dims);
   CHECK(!state->common.request.settings.monitor);
   native_wl_no_save("the first X row", &saves);

   /* Never resized: the size it had, and it gets no saved one */
   r.settings.pos     = 0;
   r.settings.pos_set = false;
   r.settings.dims    = 0;
   native_wl_replace(ctx, data, &r, window);
   state = (wl_video_window_t*)window->state;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("made automatic", &saves);
   dims = state->dims;
   native_wl_toggle(ctx, data, &r, window, dims);
   CHECK(!state->common.request.settings.dims);

   native_wl_replace(ctx, data, request, window);
}

/* The size the user leaves a window at is saved once it has settled:
 * the surface's, never a position, and never the compositor's first
 * size, a maximised one or the one it was made at */
static void native_wl_saves(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      bool kwin)
{
   static bool done;
   video_window_request_t r;
   video_window_event_t saved;
   wl_video_window_t *state;
   output_info_t *oi;
   retro_time_t changed;
   unsigned i;
   unsigned saves             = native_window_saves;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (done)
      return;
   done                  = true;
   r                     = *request;
   r.settings.monitor    = 0;
   r.settings.fullscreen = false;
   r.settings.dims       = VIDEO_SCALE_PACK(360, 270);

   /* The first configure is the compositor's, however late it is taken.
    * The harness's KWin has a rule that sizes Screen 7's window then. */
   r.screens    = 1u << 6;
   ctx->windows->destroy(data, window);
   r.generation = video_driver_window_generation();
   CHECK(ctx->windows->create(data, &r, window));
   retro_sleep(VIDEO_WINDOW_SETTLE_USEC / 1000 + 100);
   native_window_wait(ctx, data, &r, window, NATIVE_EVENT(RESUME), 0);
   state = (wl_video_window_t*)window->state;
   fprintf(stderr, "native wayland: first configured late at %ux%u\n",
         VIDEO_SCALE_W(state->dims), VIDEO_SCALE_H(state->dims));
   native_wl_map(ctx, data, window);
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("first configured late", &saves);

   /* Mapped and settled: nothing */
   r.screens = request->screens;
   native_wl_replace(ctx, data, &r, window);
   state     = (wl_video_window_t*)window->state;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("settling", &saves);
   CHECK(state->dims == r.settings.dims);

   /* Resized by the user: the surface's size, not the buffer's (at
    * scale 2 here), once */
   if (kwin)
   {
      native_wl_rescale(state, 2);
      native_window_settle(ctx, data, &r, 50000);
      native_wl_no_save("rescaled", &saves);
      native_wl_kwin(video_window_title_screen(&r),
            "var g = w.frameGeometry, c = w.clientGeometry;"
            " w.frameGeometry = {x: g.x, y: g.y,"
            " width: 390 + g.width - c.width,"
            " height: 280 + g.height - c.height};");
      CHECK(native_wl_wait_save(ctx, data, &r));
      CHECK(!native_window_saved.save_moved);
      CHECK(native_window_saved.save_pos == 0);
      CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(390, 280));
      fprintf(stderr, "native wayland: saved %ux%u, buffer %ux%u\n",
            VIDEO_SCALE_W(native_window_saved.save_dims),
            VIDEO_SCALE_H(native_window_saved.save_dims),
            VIDEO_SCALE_W(window->dims), VIDEO_SCALE_H(window->dims));
      CHECK(window->dims == native_wl_buffer(state, 390, 280));
      CHECK(window->dims != native_window_saved.save_dims);
      saves = native_window_saves;
      native_wl_rescale(state, 1);
      native_window_settle(ctx, data, &r, 200000);
      native_wl_no_save("resized again", &saves);
   }

   /* Maximised and restored, by KWin and by the window: nothing */
   for (i = 0; i < 2; i++)
   {
      unsigned was = state->dims;
      bool by_kwin = kwin && !i;
      native_wl_maximise(ctx, data, &r, window, true, by_kwin);
      native_window_settle(ctx, data, &r, 300000);
      CHECK(state->dims != was);
      native_wl_no_save("maximised", &saves);
      native_wl_maximise(ctx, data, &r, window, false, by_kwin);
      native_window_wait(ctx, data, &r, window, 0,
            native_wl_buffer(state, VIDEO_SCALE_W(was), VIDEO_SCALE_H(was)));
      native_window_settle(ctx, data, &r, 200000);
      CHECK(state->dims == was);
      native_wl_no_save("restored", &saves);
   }

   if (!kwin)
   {
      native_wl_replace(ctx, data, request, window);
      return;
   }

   /* Settled again, the user's size is saved */
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   native_wl_no_save("settling again", &saves);
   native_wl_kwin(video_window_title_screen(&r),
         "var g = w.frameGeometry, c = w.clientGeometry;"
         " w.frameGeometry = {x: g.x, y: g.y,"
         " width: 400 + g.width - c.width,"
         " height: 300 + g.height - c.height};");
   CHECK(native_wl_wait_save(ctx, data, &r));
   CHECK(!native_window_saved.save_moved);
   CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(400, 300));
   saved = native_window_saved;
   saves = native_window_saves;

   /* Saved bounds make the window they were saved from */
   r.settings.dims = saved.save_dims;
   native_wl_replace(ctx, data, &r, window);
   state = (wl_video_window_t*)window->state;
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   CHECK(state->dims == saved.save_dims);
   CHECK(window->dims == native_wl_buffer(state,
            VIDEO_SCALE_W(saved.save_dims), VIDEO_SCALE_H(saved.save_dims)));
   native_wl_no_save("remade", &saves);

   /* Resized as an output changes, as a compositor may: nothing, until
    * it has settled again. The change comes as libwayland would hand
    * it; KWin's resize must land inside the settle to tell. */
   changed = cpu_features_get_time_usec();
   oi      = native_wl_output(wl, 1);
   ((const struct wl_output_listener*)wl_proxy_get_listener(
         (struct wl_proxy*)oi->output))->done(oi, oi->output);
   native_wl_kwin(video_window_title_screen(&r),
         "var g = w.frameGeometry, c = w.clientGeometry;"
         " w.frameGeometry = {x: g.x, y: g.y,"
         " width: 380 + g.width - c.width,"
         " height: 290 + g.height - c.height};");
   while (     state->dims != VIDEO_SCALE_PACK(380, 290)
         && cpu_features_get_time_usec() < changed
            + VIDEO_WINDOW_SETTLE_USEC)
   {
      native_window_drain(ctx, data, &r);
      retro_sleep(2);
   }
   if (state->dims == VIDEO_SCALE_PACK(380, 290))
      native_wl_no_save("resized as an output changed", &saves);
   else
      fprintf(stderr, "native wayland: output change not checked,"
            " KWin's resize came after the settle\n");
   native_window_settle(ctx, data, &r, VIDEO_WINDOW_SETTLE_USEC + 100000);
   saves = native_window_saves;
   native_wl_kwin(video_window_title_screen(&r),
         "var g = w.frameGeometry, c = w.clientGeometry;"
         " w.frameGeometry = {x: g.x, y: g.y,"
         " width: 370 + g.width - c.width,"
         " height: 280 + g.height - c.height};");
   CHECK(native_wl_wait_save(ctx, data, &r));
   CHECK(native_window_saved.save_dims == VIDEO_SCALE_PACK(370, 280));

   native_wl_replace(ctx, data, request, window);
}

static void native_wl_lifecycle(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window)
{
   native_wl_main_t main_window;
   video_window_request_t other;
   video_window_native_t invalid;
   output_info_t *was;
   unsigned i, seen, last;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl_video_window_t *state   = (wl_video_window_t*)window->state;
   unsigned count             = (unsigned)wl_list_length(&wl->all_outputs);
   bool kwin                  = native_wl_kwin_dir() != NULL;
   bool decorated             = false;
#ifdef HAVE_THREADS
   bool focused;
#endif
#ifdef HAVE_LIBDECOR_H
   decorated                  = wl->libdecor != NULL;
#endif

   fprintf(stderr, "native wayland: %u output(s), %s, %s\n", count,
         decorated ? "libdecor" : "xdg-shell",
         kwin ? "KWin scripted" : "no compositor scripting");
   /* The harness's nested KWin has two, so the output choice shows */
   CHECK(!kwin || count >= 2);
   /* Published at init, before anything pumped */
   {
      video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
      CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
            == native_wl_outputs(wl));
   }
   native_wl_main_get(wl, &main_window);
   CHECK(window->kind == VIDEO_WINDOW_NATIVE_WAYLAND);
   CHECK(window->display == wl->input.dpy);
   CHECK(window->surface == state->surface && window->surface != wl->surface);
   /* Configured by later event processing, never waited for; nothing
    * goes on the surface before */
   CHECK(!window->configured && !window->presentable);
   if (ctx->windows->bind_gl)
   {
      CHECK(ctx->windows->bind_gl(data, window));
      CHECK(!ctx->windows->present_gl(data, window));
      CHECK(ctx->windows->bind_gl(data, NULL));
   }
   /* A size asked for meanwhile is the one the first configure gives */
   request->settings.dims = VIDEO_SCALE_PACK(300, 200);
   ctx->windows->update(data, window, request);
   /* After a restart, what was queued for the last lifetime's window
    * (a close, a resize, focus) has to reach nothing */
   seen  = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 100000);
   CHECK(!(seen & (NATIVE_EVENT(CLOSE) | NATIVE_EVENT(FOCUS_GAIN)
               | NATIVE_EVENT(FOCUS_LOSS))));
   CHECK(window->configured && window->presentable && !window->focused);
   CHECK(state->dims == VIDEO_SCALE_PACK(300, 200));
   CHECK(window->dims == native_wl_buffer(state, 300, 200));
   CHECK(native_wl_main_intact(wl, &main_window));

   native_wl_map(ctx, data, window);
   native_window_settle(ctx, data, request, 200000);
   CHECK(native_wl_output_of(wl, state) != NULL);
   fprintf(stderr, "native wayland: %ux%u buffer for 300x200, scale %u,"
         " fractional %u/120\n", VIDEO_SCALE_W(window->dims),
         VIDEO_SCALE_H(window->dims), state->scale,
         state->fractional_scale ? state->fractional_scale_num : 0);

   /* The title as the compositor has it: exactly this screen's, on one
    * window only, which KWin then sizes to say so */
   if (native_wl_kwin(video_window_title_screen(request),
            "if (n == 1) { var g = w.frameGeometry, c = w.clientGeometry;"
            " w.frameGeometry = {x: g.x, y: g.y,"
            " width: 380 + g.width - c.width,"
            " height: 290 + g.height - c.height}; }"))
   {
      if (!(native_window_wait(ctx, data, request, window,
                  NATIVE_EVENT(GEOMETRY), native_wl_buffer(state, 380, 290))
               & NATIVE_EVENT(GEOMETRY)))
         fprintf(stderr, "native wayland: no window captioned \""
               NATIVE_TITLE "%u\" alone\n",
               video_window_title_screen(request) + 1);
   }
   else
      fprintf(stderr, "native wayland: titles not readable without"
            " KWin\n");

   /* Resized by the compositor, then by the frontend */
   if (native_wl_kwin(video_window_title_screen(request),
            "var g = w.frameGeometry, c = w.clientGeometry;"
            " w.frameGeometry = {x: g.x, y: g.y,"
            " width: 400 + g.width - c.width,"
            " height: 300 + g.height - c.height};"))
   {
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
            native_wl_buffer(state, 400, 300));
      CHECK(state->dims == VIDEO_SCALE_PACK(400, 300));
   }
   request->settings.dims = VIDEO_SCALE_PACK(360, 270);
   ctx->windows->update(data, window, request);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(GEOMETRY),
         native_wl_buffer(state, 360, 270));
   CHECK(state->dims == VIDEO_SCALE_PACK(360, 270));

   /* Fullscreen on the last output, then the first, then a window
    * again at the size it had */
   native_wl_fullscreen(ctx, data, request, window, count,
         native_wl_output(wl, count));
   native_wl_fullscreen(ctx, data, request, window, 1,
         native_wl_output(wl, 1));
   /* A size asked for meanwhile is the one it floats at again, not the
    * compositor's restore size */
   request->settings.dims = VIDEO_SCALE_PACK(340, 260);
   ctx->windows->update(data, window, request);
   native_window_settle(ctx, data, request, 50000);
   CHECK(state->fullscreen);
   native_wl_windowed(ctx, data, request, window);

   /* A saved output that is gone: the one the window is on */
   was = native_wl_output_of(wl, state);
   native_wl_fullscreen(ctx, data, request, window, count + 3, was);
   native_wl_windowed(ctx, data, request, window);
   CHECK(native_wl_main_intact(wl, &main_window));

   /* A request of an older generation changes nothing */
   native_window_settle(ctx, data, request, 100000);
   other                     = *request;
   other.generation          = request->generation - 1;
   other.settings.dims       = VIDEO_SCALE_PACK(200, 150);
   other.settings.fullscreen = true;
   ctx->windows->update(data, window, &other);
   seen = native_window_settle(ctx, data, request, 200000);
   CHECK(!(seen & NATIVE_EVENT(GEOMETRY)));
   CHECK(state->dims == request->settings.dims && !state->fullscreen);

   /* Minimised, then restored by the compositor */
   if (kwin)
   {
#ifdef HAVE_LIBDECOR_H
      if (state->libdecor_frame)
         wl->libdecor_frame_set_minimized(state->libdecor_frame);
      else
#endif
         xdg_toplevel_set_minimized(state->xdg_toplevel);
      wl_display_flush(wl->input.dpy);
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(SUSPEND), 0);
      CHECK(!window->presentable);
      CHECK(!ctx->windows->present_gl
            || !ctx->windows->present_gl(data, window));
      native_wl_kwin(video_window_title_screen(request),
            "w.minimized = false;");
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
      CHECK(window->presentable);
   }

   native_gl_binding(ctx, data, window);
   CHECK(native_wl_main_intact(wl, &main_window));
   /* Its own first frames (GL) put the main window on an output */
   native_window_settle(ctx, data, request, 200000);
   native_wl_main_get(wl, &main_window);
   native_wl_monitors(ctx, data, request);
   CHECK(native_wl_main_intact(wl, &main_window));

   /* The seat's focus, on the frontend's thread; focus going between
    * the main window and a secondary keeps RetroArch focused. The main
    * window is shown once it has had frames (GL); otherwise focus
    * leaves as the window is minimised. */
   if (kwin)
   {
      native_wl_wait_focus(ctx, data, request, window, true,
            "workspace.activeWindow = w;");
      CHECK(native_wl_keyboard_focus(wl));
      if (ctx->windows->bind_gl)
      {
         native_wl_wait_focus(ctx, data, request, window, false,
               "workspace.activeWindow = m;");
         CHECK(native_wl_keyboard_focus(wl));
         native_wl_wait_focus(ctx, data, request, window, true,
               "workspace.activeWindow = w;");
         CHECK(native_wl_keyboard_focus(wl));
         native_wl_wait_focus(ctx, data, request, window, false,
               "workspace.activeWindow = m;");
      }
      else
      {
         native_wl_wait_focus(ctx, data, request, window, false,
               "w.minimized = true;");
         native_wl_kwin(video_window_title_screen(request),
               "w.minimized = false;");
         native_window_wait(ctx, data, request, window,
               NATIVE_EVENT(RESUME), 0);
      }
   }

   /* A focus loss outlasts a flood the queue has no room for, and the
    * focus ends where the seat left it */
   native_window_settle(ctx, data, request, 200000);
#ifdef HAVE_THREADS
   focused = window->focused;
#endif
   native_wl_flood(wl, (struct wl_surface*)window->surface, 100, true, true,
         true);
#ifdef HAVE_THREADS
   /* Queued for the context's thread, not applied on the seat's */
   CHECK(window->focused == focused);
#else
   CHECK(window->focused);
#endif
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
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(seen & NATIVE_EVENT(FOCUS_GAIN));
   CHECK(window->focused);
   native_wl_flood(wl, (struct wl_surface*)window->surface, 100, true, false,
         true);
   seen = native_window_drain(ctx, data, request);
   CHECK(seen & NATIVE_EVENT(FOCUS_LOSS));
   CHECK(!window->focused);
   /* The seat's events on the context's own thread apply at once */
   native_wl_flood(wl, (struct wl_surface*)window->surface, 1, false, false,
         false);
   CHECK(window->focused);
   native_wl_flood(wl, (struct wl_surface*)window->surface, 0, true, false,
         false);
   CHECK(!window->focused);
   native_window_drain(ctx, data, request);
   CHECK(native_wl_main_intact(wl, &main_window));

   native_wl_pointer(wl, (struct wl_surface*)window->surface);

   /* Closing the focused window takes its focus with it */
   native_wl_focus(ctx, data, request, window);
   native_wl_close_focused(ctx, data, request, window);
   CHECK(ctx->windows->bind_gl == NULL
         || !ctx->windows->bind_gl(data, window));
   CHECK(ctx->windows->present_gl == NULL
         || !ctx->windows->present_gl(data, window));

   /* What is still queued for the closed window - a resize and a close
    * by the compositor, focus by the seat - cannot reach the one that
    * replaces it */
   native_wl_kwin(video_window_title_screen(request),
         "var g = w.frameGeometry;"
         " w.frameGeometry = {x: g.x, y: g.y, width: 500, height: 400};"
         " w.closeWindow();");
   native_wl_flood(wl, (struct wl_surface*)window->surface, 100, true, true,
         true);
   last = request->generation;
   native_wl_pointer_gone(ctx, data, wl, window);
   CHECK(window->state == NULL && window->surface == NULL);
   native_wl_pointer_alone(wl);
   request->generation = video_driver_window_generation();
   CHECK(request->generation > last);
   CHECK(ctx->windows->create(data, request, window));
   CHECK(window->generation == request->generation);
   state = (wl_video_window_t*)window->state;
   seen  = native_window_wait(ctx, data, request, window,
         NATIVE_EVENT(RESUME), 0);
   seen |= native_window_settle(ctx, data, request, 100000);
   CHECK(!(seen & (NATIVE_EVENT(CLOSE) | NATIVE_EVENT(FOCUS_GAIN)
               | NATIVE_EVENT(FOCUS_LOSS))));
   CHECK(window->presentable && !window->focused);
   CHECK(state->dims == request->settings.dims);

   /* The compositor's close button */
   native_wl_map(ctx, data, window);
   native_window_settle(ctx, data, request, 100000);
   native_wl_close(wl, state);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
   CHECK(native_window_closed_user);
   CHECK(!window->presentable);
   CHECK(native_wl_main_intact(wl, &main_window));
   ctx->windows->destroy(data, window);

   /* Game Screen 1's window on a spare id: captioned for its screen, on
    * one window only, which KWin then sizes to say so */
   {
      video_window_request_t spare;
      video_window_native_t one;
      native_window_spare(request, &spare);
      memset(&one, 0, sizeof(one));
      CHECK(ctx->windows->create(data, &spare, &one));
      native_window_wait(ctx, data, &spare, &one, NATIVE_EVENT(RESUME), 0);
      native_wl_map(ctx, data, &one);
      native_window_settle(ctx, data, &spare, 100000);
      if (     one.state
            && native_wl_kwin(0,
               "if (n == 1) { var g = w.frameGeometry, c = w.clientGeometry;"
               " w.frameGeometry = {x: g.x, y: g.y,"
               " width: 390 + g.width - c.width,"
               " height: 280 + g.height - c.height}; }")
            && !(native_window_wait(ctx, data, &spare, &one,
                  NATIVE_EVENT(GEOMETRY),
                  native_wl_buffer((wl_video_window_t*)one.state, 390, 280))
               & NATIVE_EVENT(GEOMETRY)))
         fprintf(stderr, "native wayland: no window captioned \""
               NATIVE_TITLE "1\" alone\n");
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
   CHECK(ctx->windows->create(data, request, window));
   CHECK(!ctx->windows->create(data, request, &invalid));
   CHECK(!invalid.state && !invalid.surface);
   native_window_wait(ctx, data, request, window, NATIVE_EVENT(RESUME), 0);
#ifdef NATIVE_EGL
   if (ctx->windows->bind_gl)
      native_egl_failures(ctx, data, request, window, native_wl_drawable);
#endif
   native_wl_map(ctx, data, window);
   native_window_settle(ctx, data, request, 100000);
   CHECK(native_wl_main_intact(wl, &main_window));

   native_wl_saves(ctx, data, request, window, kwin);
   native_wl_updates(ctx, data, request, window, kwin);
   CHECK(native_wl_main_intact(wl, &main_window));
}

/* A context destroyed for good takes its list down, on its own thread.
 * A kept one let go later stays quiet: by then the next driver may
 * have published, from another thread. */
static void native_wl_teardown(const char *ident, enum gfx_ctx_api api,
      bool core)
{
   static video_window_display_t foreign[1];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX];
   const gfx_ctx_driver_t *ctx;
   void *data = NULL;
   memset(foreign, 0, sizeof(foreign));
   foreign[0].ordinal = 1;
   foreign[0].valid   = foreign[0].on = foreign[0].physical = true;
   foreign[0].monitor = true;
   strlcpy(foreign[0].name, "foreign", sizeof(foreign[0].name));
   video_driver_publish_window_displays(foreign, 1);
   gfx_ctx_wl_release_kept();
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 1 && string_is_equal(got[0].name, "foreign"));
#ifdef HAVE_VULKAN
   if (api == GFX_CTX_VULKAN_API)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_vk_wayland, ident, api, 1, 0, false, &data);
   else
#endif
   ctx = video_context_driver_init_first(NULL, ident, api,
         core ? 3 : 0, core ? 2 : 0, false, &data);
   CHECK(ctx != NULL);
   if (!ctx)
      return;
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == native_wl_outputs((gfx_ctx_wayland_data_t*)data));
   /* Quitting: not kept */
   runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_SHUTDOWN_INITIATED;
   ctx->destroy(data);
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_SHUTDOWN_INITIATED;
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 0);
}
#endif

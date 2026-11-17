/* What every backend of video_screens_test.c's native-window
 * lifecycle shares; #included there. */
#ifdef NATIVE_WINDOWS
/* Secondary windows through a context's optional table. The deadlines
 * below are the fixture's own; the shipping code never waits. */
static unsigned native_window_pos;
/* The last GEOMETRY with bounds to save, and how many there were */
static video_window_event_t native_window_saved;
static unsigned native_window_saves;
/* The last CLOSE's user flag */
static bool native_window_closed_user;
/* The frontend's input poll, where this one thread runs it too */
static void (*native_window_input)(void *data);

static unsigned native_window_drain(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request)
{
   video_window_event_t events[4];
   unsigned i, count;
   unsigned seen = 0;
   unsigned dims = VIDEO_SCALE_PACK(640, 480);
   bool quit     = false;
   bool resize   = false;
   if (native_window_input)
      native_window_input(data);
   ctx->check_window(data, &quit, &resize, &dims);
   CHECK(!quit);
   while ((count = ctx->windows->poll(data, events,
               sizeof(events) / sizeof(*events))))
      for (i = 0; i < count; i++)
      {
         CHECK(events[i].screen == request->screen);
         CHECK(events[i].generation == request->generation);
         if (events[i].kind == VIDEO_WINDOW_EVENT_GEOMETRY)
         {
            native_window_pos = events[i].pos;
            if (events[i].save_dims)
            {
               native_window_saved = events[i];
               native_window_saves++;
            }
         }
         else if (events[i].kind == VIDEO_WINDOW_EVENT_CLOSE)
            native_window_closed_user = events[i].user;
         seen |= 1u << events[i].kind;
      }
   return seen;
}

/* Pumps until every kind in 'wanted' arrived and, with 'dims' set,
 * the window has that size. */
static unsigned native_window_wait(const gfx_ctx_driver_t *ctx, void *data,
      const video_window_request_t *request,
      const video_window_native_t *window, unsigned wanted, unsigned dims)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   unsigned seen         = 0;
   for (;;)
   {
      seen |= native_window_drain(ctx, data, request);
      if ((seen & wanted) == wanted && (!dims || window->dims == dims))
         return seen;
      if (cpu_features_get_time_usec() > deadline)
         break;
      retro_sleep(2);
   }
   fprintf(stderr, "native window: wanted 0x%x, saw 0x%x, size %ux%u\n",
         wanted, seen, VIDEO_SCALE_W(window->dims),
         VIDEO_SCALE_H(window->dims));
   CHECK((seen & wanted) == wanted);
   CHECK(!dims || window->dims == dims);
   return seen;
}

/* Pumps for 'usec' and reports what arrived. */
static unsigned native_window_settle(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request, retro_time_t usec)
{
   retro_time_t deadline = cpu_features_get_time_usec() + usec;
   unsigned seen         = 0;
   do
   {
      seen |= native_window_drain(ctx, data, request);
      retro_sleep(2);
   } while (cpu_features_get_time_usec() < deadline);
   return seen;
}

/* Pumps until the context's has_focus reads 'want'; whether it did */
static bool native_window_has_focus(const gfx_ctx_driver_t *ctx,
      void *data, const video_window_request_t *request, bool want)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;
   while (     ctx->has_focus(data) != want
         && cpu_features_get_time_usec() < deadline)
   {
      native_window_drain(ctx, data, request);
      retro_sleep(2);
   }
   return ctx->has_focus(data) == want;
}

#define NATIVE_EVENT(kind) (1u << VIDEO_WINDOW_EVENT_##kind)

#define NATIVE_TITLE "RetroArch - Screen "

/* A finder takes a secondary by its exact title, and the main window
 * as the one without such a title: what the window system has for
 * screen 'screen''s window, and for the main window ("" for none), read
 * back. */
static void native_window_titles(const char *title, const char *main_title,
      unsigned screen)
{
   char want[64];
   snprintf(want, sizeof(want), NATIVE_TITLE "%u", screen + 1);
   if (!title || strcmp(title, want) != 0 || !main_title
         || strncmp(main_title, NATIVE_TITLE, strlen(NATIVE_TITLE)) == 0)
      fprintf(stderr, "native window: title \"%s\", main \"%s\"\n",
            title ? title : "(none)", main_title ? main_title : "(none)");
   CHECK(title && strcmp(title, want) == 0);
   CHECK(main_title
         && strncmp(main_title, NATIVE_TITLE, strlen(NATIVE_TITLE)) != 0);
}

/* Game Screen 1's window as it comes on its own: an id past the
 * content's screens, showing screen 0 */
static void native_window_spare(const video_window_request_t *request,
      video_window_request_t *spare)
{
   *spare            = *request;
   spare->screen     = 2;
   spare->screens    = 1u;
   spare->generation = video_driver_window_generation();
}

#ifdef HAVE_VULKAN
/* The surface and swapchain vulkan.c makes for a screen window */
static void native_vk_output(const gfx_ctx_driver_t *ctx, void *data,
      video_window_native_t *window)
{
   VkAttachmentDescription color;
   VkAttachmentReference ref;
   VkSubpassDescription subpass;
   VkRenderPassCreateInfo info;
   vulkan_output_t output;
   vulkan_context_t *vk = ctx->get_context_data
      ? (vulkan_context_t*)ctx->get_context_data(data) : NULL;

   CHECK(vk != NULL && vk->device != VK_NULL_HANDLE);
   if (!vk || vk->device == VK_NULL_HANDLE)
      return;
   color.flags                     = 0;
   color.format                    = vk->swapchain_format;
   color.samples                   = VK_SAMPLE_COUNT_1_BIT;
   color.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
   color.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
   color.stencilLoadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   color.stencilStoreOp            = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   color.initialLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
   color.finalLayout               = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   ref.attachment                  = 0;
   ref.layout                      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   subpass.flags                   = 0;
   subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.inputAttachmentCount    = 0;
   subpass.pInputAttachments       = NULL;
   subpass.colorAttachmentCount    = 1;
   subpass.pColorAttachments       = &ref;
   subpass.pResolveAttachments     = NULL;
   subpass.pDepthStencilAttachment = NULL;
   subpass.preserveAttachmentCount = 0;
   subpass.pPreserveAttachments    = NULL;
   info.sType                      = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   info.pNext                      = NULL;
   info.flags                      = 0;
   info.attachmentCount            = 1;
   info.pAttachments               = &color;
   info.subpassCount               = 1;
   info.pSubpasses                 = &subpass;
   info.dependencyCount            = 0;
   info.pDependencies              = NULL;
   memset(&output, 0, sizeof(output));
   if (vkCreateRenderPass(vk->device, &info, NULL, &output.render_pass)
         != VK_SUCCESS)
   {
      CHECK(!"render pass");
      return;
   }
   CHECK(vulkan_output_create(&output, vk,
         VULKAN_CTX_DATA_FROM_CONTEXT(vk)->wsi_type, window->display,
         window->surface, window->dims));
   CHECK(output.swapchain != VK_NULL_HANDLE);
   vulkan_output_finish(&output);
   CHECK(vulkan_output_retire(&output));
   vkDestroyRenderPass(vk->device, output.render_pass, NULL);
}
#endif

static void native_gl_binding(const gfx_ctx_driver_t *ctx, void *data,
      video_window_native_t *window)
{
   if (!ctx->windows->bind_gl)
   {
      CHECK(ctx->windows->present_gl == NULL);
#ifdef HAVE_VULKAN
      if (strncmp(ctx->ident, "vk_", 3) == 0)
         native_vk_output(ctx, data, window);
#endif
      return;
   }
#ifdef NATIVE_GLX
   if (strcmp(ctx->ident, "x") == 0)
   {
      Display *dpy         = (Display*)window->display;
      GLXDrawable primary  = glXGetCurrentDrawable();
      GLXContext context   = glXGetCurrentContext();
      const char *exts     = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
      bool query           = exts && strstr(exts, "GLX_EXT_swap_control");
      unsigned interval    = 0;
      unsigned value       = 0;
      GLXDrawable drawable;
      CHECK(primary != None && context != NULL);
      if (query)
         glXQueryDrawable(dpy, primary, GLX_SWAP_INTERVAL_EXT, &interval);
      CHECK(ctx->windows->bind_gl(data, window));
      drawable = glXGetCurrentDrawable();
      CHECK(drawable != None && drawable != primary);
      CHECK(glXGetCurrentContext() == context);
      if (query)
      {
         glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &value);
         CHECK(value == 0);
      }
      CHECK(ctx->windows->present_gl(data, window));
      CHECK(ctx->windows->bind_gl(data, NULL));
      CHECK(glXGetCurrentDrawable() == primary);
      CHECK(glXGetCurrentContext() == context);
      if (query)
      {
         glXQueryDrawable(dpy, primary, GLX_SWAP_INTERVAL_EXT, &value);
         CHECK(value == interval && interval == 1);
      }
      return;
   }
#endif
#ifdef NATIVE_EGL
   if (     strcmp(ctx->ident, "egl_x") == 0
         || strcmp(ctx->ident, "wayland") == 0)
   {
      EGLSurface primary = eglGetCurrentSurface(EGL_DRAW);
      EGLContext context = eglGetCurrentContext();
      EGLDisplay dpy     = eglGetCurrentDisplay();
      bool wayland       = strcmp(ctx->ident, "wayland") == 0;
      EGLSurface surface;
      retro_time_t start;
      unsigned i;
      CHECK(primary != EGL_NO_SURFACE && context != EGL_NO_CONTEXT);
      CHECK(ctx->windows->bind_gl(data, window));
      surface = eglGetCurrentSurface(EGL_DRAW);
      CHECK(surface != EGL_NO_SURFACE && surface != primary);
      CHECK(eglGetCurrentContext() == context);
      CHECK(ctx->windows->present_gl(data, window));
      /* Interval 0: no swap waits for the compositor's frame callback,
       * which comes once a refresh at best (and never while hidden) */
      start = cpu_features_get_time_usec();
      for (i = 0; wayland && i < 20; i++)
         CHECK(ctx->windows->present_gl(data, window));
      CHECK(cpu_features_get_time_usec() - start < 250000);
      CHECK(ctx->windows->bind_gl(data, NULL));
      CHECK(eglGetCurrentSurface(EGL_DRAW) == primary);
      CHECK(eglGetCurrentContext() == context);
      /* The main window's interval 1 still waits for them */
      start = cpu_features_get_time_usec();
      for (i = 0; wayland && i < 4; i++)
         CHECK(eglSwapBuffers(dpy, primary));
      CHECK(!wayland || cpu_features_get_time_usec() - start > 30000);
      return;
   }
#endif
#ifdef NATIVE_WGL
   if (strcmp(ctx->ident, "wgl") == 0)
   {
      typedef int (WINAPI *get_interval_t)(void);
      HDC primary   = wglGetCurrentDC();
      HGLRC context = wglGetCurrentContext();
      get_interval_t get_interval = (get_interval_t)
         wglGetProcAddress("wglGetSwapIntervalEXT");
      int interval  = 0;
      unsigned i;
      HDC dc;
      CHECK(primary != NULL && context != NULL);
      fprintf(stderr, "native wgl: %s\n", get_interval
            ? "intervals queried" : "no wglGetSwapIntervalEXT");
      if (get_interval)
         interval = get_interval();
      /* Twice: the secondary's interval is set once and holds */
      for (i = 0; i < 2; i++)
      {
         CHECK(ctx->windows->bind_gl(data, window));
         dc = wglGetCurrentDC();
         CHECK(dc != NULL && dc != primary);
         CHECK(WindowFromDC(dc) == (HWND)window->surface);
         CHECK(wglGetCurrentContext() == context);
         CHECK(!get_interval || get_interval() == 0);
         CHECK(ctx->windows->present_gl(data, window));
         CHECK(ctx->windows->bind_gl(data, NULL));
         CHECK(wglGetCurrentDC() == primary);
         CHECK(wglGetCurrentContext() == context);
         CHECK(!get_interval || (get_interval() == interval && interval == 1));
      }
      return;
   }
#endif
   CHECK(ctx->windows->bind_gl(data, window));
   CHECK(ctx->windows->present_gl(data, window));
   CHECK(ctx->windows->bind_gl(data, NULL));
}

#ifdef NATIVE_EGL
/* A secondary EGL surface that fails fails its window, which is then
 * replaced: a present with another surface current fails its swap, and
 * a bind of a surface destroyed underneath fails to make it current,
 * which puts the main window's surface back whatever was current (none,
 * here, where the context can go without one). Each gives CLOSE. */
static void native_egl_failures(const gfx_ctx_driver_t *ctx, void *data,
      video_window_request_t *request, video_window_native_t *window,
      void *(*surface)(const video_window_native_t *window))
{
   unsigned i;
   EGLSurface primary = eglGetCurrentSurface(EGL_DRAW);
   EGLDisplay dpy     = eglGetCurrentDisplay();
   for (i = 0; i < 2; i++)
   {
      CHECK(window->presentable);
      if (!i)
         CHECK(!ctx->windows->present_gl(data, window));
      else
      {
         CHECK(eglDestroySurface(dpy, (EGLSurface)surface(window)));
         if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                  eglGetCurrentContext()))
            fprintf(stderr, "native egl: no surfaceless context, the main"
                  " surface stays current\n");
         CHECK(!ctx->windows->bind_gl(data, window));
      }
      native_window_wait(ctx, data, request, window, NATIVE_EVENT(CLOSE), 0);
      CHECK(!native_window_closed_user);
      CHECK(!window->presentable);
      CHECK(!ctx->windows->bind_gl(data, window));
      CHECK(eglGetCurrentSurface(EGL_DRAW) == primary);
      ctx->windows->destroy(data, window);
      request->generation = video_driver_window_generation();
      CHECK(ctx->windows->create(data, request, window));
      native_window_wait(ctx, data, request, window,
            NATIVE_EVENT(RESUME), 0);
   }
}
#endif
#endif

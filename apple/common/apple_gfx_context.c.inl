#include "../../gfx/gfx_common.h"
#include "../../gfx/gfx_context.h"
#include "../../gfx/gl_common.h"

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static bool g_use_hw_ctx;

#ifdef OSX
static bool g_has_went_fullscreen;
static NSOpenGLPixelFormat* g_format;
static NSOpenGLContext *g_hw_ctx;
#endif

static unsigned g_minor = 0;
static unsigned g_major = 0;

static RAScreen* get_chosen_screen(void)
{
#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_6)
	return [NSScreen mainScreen];
#else
   if (g_settings.video.monitor_index >= RAScreen.screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.\n");
      return RAScreen.mainScreen;
   }
	
   return (RAScreen*)[RAScreen.screens objectAtIndex:g_settings.video.monitor_index];
#endif
}

static bool apple_gfx_ctx_init(void *data)
{
   (void)data;
    
    RARCH_LOG("Minor: %d\n", g_minor);
    RARCH_LOG("Major: %d\n", g_major);
    
#ifdef OSX
    
    NSOpenGLPixelFormatAttribute attributes [] = {
        NSOpenGLPFADoubleBuffer,	// double buffered
        NSOpenGLPFADepthSize,
        (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
#ifdef MAC_OS_X_VERSION_10_7
        (g_major || g_minor) ? NSOpenGLPFAOpenGLProfile : 0,
        (g_major << 12) | (g_minor << 8),
#endif
        (NSOpenGLPixelFormatAttribute)nil
    };
    
    g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    
    if (g_use_hw_ctx)
    {
        //g_hw_ctx  = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
        g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:g_hw_ctx];
    }
    else
    {
       g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
    }
    [g_context setView:g_view];
#else
    g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    g_view.context = g_context;
#endif
    
    [g_context makeCurrentContext];
   // Make sure the view was created
   [RAGameView get];
   return true;
}

static void apple_gfx_ctx_destroy(void *data)
{
   (void)data;

   [GLContextClass clearCurrentContext];

#if defined(IOS)
   g_view.context = nil;
#elif defined(OSX)
    [g_context clearDrawable];
    if (g_context)
        [g_context release];
    g_context = nil;
    if (g_format)
        [g_format release];
    g_format = nil;
    if (g_hw_ctx)
        [g_hw_ctx release];
    g_hw_ctx = nil;
#endif
   [GLContextClass clearCurrentContext];
   g_context = nil;
}



static bool apple_gfx_ctx_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   if (api != GLAPIType)
      return false;
    
   g_minor = minor;
   g_major = major;
  
   return true;
}

static void apple_gfx_ctx_swap_interval(void *data, unsigned interval)
{
   (void)data;
#ifdef IOS // < No way to disable Vsync on iOS?
           //   Just skip presents so fast forward still works.
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(OSX)
   GLint value = interval ? 1 : 0;
   [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];
#endif
}

static bool apple_gfx_ctx_set_video_mode(void *data, unsigned width, unsigned height, bool fullscreen)
{
   (void)data;
#ifdef OSX
   // TODO: Screen mode support
   
   if (fullscreen && !g_has_went_fullscreen)
   {
      [g_view enterFullScreenMode:get_chosen_screen() withOptions:nil];
      [NSCursor hide];
   }
   else if (!fullscreen && g_has_went_fullscreen)
   {
      [g_view exitFullScreenModeWithOptions:nil];
      [[g_view window] makeFirstResponder:g_view];
      [NSCursor unhide];
   }
   
   g_has_went_fullscreen = fullscreen;
   if (!g_has_went_fullscreen)
      [[g_view window] setContentSize:NSMakeSize(width, height)];
#endif

   // TODO: Maybe iOS users should be apple to show/hide the status bar here?

   return true;
}

static void apple_gfx_ctx_get_video_size(void *data, unsigned* width, unsigned* height)
{
   RAScreen *screen = (RAScreen*)get_chosen_screen();
   CGRect size = screen.bounds;
   gl_t *gl = (gl_t*)data;
	
   if (gl)
   {
#if defined(OSX)
      CGRect cgrect = NSRectToCGRect([g_view frame]);
      size = CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
#else
      size = g_view.bounds;
#endif
   }

   *width  = CGRectGetWidth(size)  * screen.scale;
   *height = CGRectGetHeight(size) * screen.scale;
}

static void apple_gfx_ctx_update_window_title(void *data)
{
   static char buf[128], buf_fps[128];
   bool fps_draw, got_text;
    
   (void)data;
   (void)got_text;

   fps_draw = g_settings.fps_show;
   got_text = gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));
   static const char* const text = buf; // < Can't access buf directly in the block
    (void)text;
#ifdef OSX
   if (got_text)
       [[g_view window] setTitle:[NSString stringWithCString:text encoding:NSUTF8StringEncoding]];
#endif
   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static bool apple_gfx_ctx_has_focus(void *data)
{
   (void)data;
   return APP_HAS_FOCUS;
}

static void apple_gfx_ctx_swap_buffers(void *data)
{
   bool swap;
   (void)data;
   swap = --g_fast_forward_skips < 0;

   if (!swap)
      return;

   [g_view display];
   g_fast_forward_skips = g_is_syncing ? 0 : 3;
}

static gfx_ctx_proc_t apple_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
#ifdef MAC_OS_X_VERSION_10_7
         (__bridge CFStringRef)BOXSTRING(symbol_name)
#else
         (CFStringRef)BOXSTRING(symbol_name)
#endif
         );
}

static void apple_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   apple_gfx_ctx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void apple_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void apple_gfx_ctx_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static void apple_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;
   g_use_hw_ctx = enable;
    
    if (enable)
        [g_hw_ctx makeCurrentContext];
    else
        [g_context makeCurrentContext];
}

// The apple_* functions are implemented in apple/RetroArch/RAGameView.m
const gfx_ctx_driver_t gfx_ctx_apple = {
   apple_gfx_ctx_init,
   apple_gfx_ctx_destroy,
   apple_gfx_ctx_bind_api,
   apple_gfx_ctx_swap_interval,
   apple_gfx_ctx_set_video_mode,
   apple_gfx_ctx_get_video_size,
   NULL,
   apple_gfx_ctx_update_window_title,
   apple_gfx_ctx_check_window,
   apple_gfx_ctx_set_resize,
   apple_gfx_ctx_has_focus,
   apple_gfx_ctx_swap_buffers,
   apple_gfx_ctx_input_driver,
   apple_gfx_ctx_get_proc_address,
   NULL,
   "apple",
   apple_gfx_ctx_bind_hw_render,
};

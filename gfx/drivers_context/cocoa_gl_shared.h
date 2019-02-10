#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if TARGET_OS_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSScreen.h>
#include <AppKit/NSOpenGL.h>
#elif defined(HAVE_COCOATOUCH)
#include <GLKit/GLKit.h>
#endif

#include <retro_assert.h>
#include <compat/apple_compat.h>

#include "../../ui/drivers/ui_cocoa.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../video_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

typedef struct cocoa_ctx_data
{
   bool core_hw_context_enable;
#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
   int swap_interval;
#endif
    unsigned width;
    unsigned height;
} cocoa_ctx_data_t;

#if defined(HAVE_COCOATOUCH)
#define GLContextClass EAGLContext
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif
#else
#define GLContextClass NSOpenGLContext
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen
#endif

static enum gfx_ctx_api cocoagl_api = GFX_CTX_NONE;

#if defined(HAVE_COCOATOUCH)
static GLKView *g_view;
UIView *g_pause_indicator_view;
#endif

static GLContextClass* g_hw_ctx;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static bool g_use_hw_ctx = false;

static unsigned g_minor  = 0;
static unsigned g_major  = 0;

#if defined(HAVE_COCOATOUCH)
@interface EAGLContext (OSXCompat) @end
@implementation EAGLContext (OSXCompat)
+ (void)clearCurrentContext { [EAGLContext setCurrentContext:nil];  }
- (void)makeCurrentContext  { [EAGLContext setCurrentContext:self]; }
@end
#else
@interface NSScreen (IOSCompat) @end
@implementation NSScreen (IOSCompat)
- (CGRect)bounds
{
   CGRect cgrect  = NSRectToCGRect(self.frame);
   return CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
}
- (float) scale  { return 1.0f; }
@end
#endif

/* forward declaration */
void *nsview_get_ptr(void);

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
static NSOpenGLPixelFormat* g_format;

void *glcontext_get_ptr(void)
{
   return (BRIDGE void *)g_context;
}
#endif

static uint32_t cocoagl_gfx_ctx_get_flags(void *data)
{
   uint32_t flags                 = 0;
   cocoa_ctx_data_t    *cocoa_ctx = (cocoa_ctx_data_t*)data;

   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);

   if (cocoa_ctx->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   return flags;
}

static void cocoagl_gfx_ctx_set_flags(void *data, uint32_t flags)
{
   (void)flags;
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      cocoa_ctx->core_hw_context_enable = true;
}

void *glkitview_init(void)
{
#if defined(HAVE_COCOATOUCH)
#if TARGET_OS_IOS
   /* iOS Pause menu and lifecycle. */
   UINib *xib = (UINib*)[UINib nibWithNibName:BOXSTRING("PauseIndicatorView") bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
#endif

   g_view = [GLKView new];
#if TARGET_OS_IOS
   g_view.multipleTouchEnabled = YES;
    [g_view addSubview:g_pause_indicator_view];
#endif
   g_view.enableSetNeedsDisplay = NO;

   return (BRIDGE void *)((GLKView*)g_view);
#else
    return nsview_get_ptr();
#endif
}

#if defined(HAVE_COCOATOUCH)
void cocoagl_bind_game_view_fbo(void)
{
   if (g_context)
      [g_view bindDrawable];
}
#endif

static float get_from_selector(Class obj_class, id obj_id, SEL selector, CGFloat *ret)
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
    [obj_class instanceMethodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    [invocation setTarget:obj_id];
    [invocation invoke];
    [invocation getReturnValue:ret];
    RELEASE(invocation);
    return *ret;
}

void *get_chosen_screen(void)
{
   settings_t *settings = config_get_ptr();
   NSArray *screens = [RAScreen screens];
   if (!screens || !settings)
      return NULL;

   if (settings->uints.video_monitor_index >= screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.");
      return (BRIDGE void*)screens;
   }

   return ((BRIDGE void*)[screens objectAtIndex:settings->uints.video_monitor_index]);
}

float get_backing_scale_factor(void)
{
   static float
   backing_scale_def = 0.0f;
   RAScreen *screen     = NULL;

   (void)screen;

   if (backing_scale_def != 0.0f)
      return backing_scale_def;

   backing_scale_def = 1.0f;
#ifdef HAVE_COCOA_METAL
   screen = (BRIDGE RAScreen*)get_chosen_screen();

   if (screen)
   {
      SEL selector = NSSelectorFromString(BOXSTRING("backingScaleFactor"));
      if ([screen respondsToSelector:selector])
      {
         CGFloat ret;
         NSView *g_view        = apple_platform.renderView;
         //CocoaView *g_view     = (CocoaView*)nsview_get_ptr();
         backing_scale_def     = (float)get_from_selector
         ([[g_view window] class], [g_view window], selector, &ret);
      }
   }
#endif

   return backing_scale_def;
}

void cocoagl_gfx_ctx_update(void)
{
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
#if MAC_OS_X_VERSION_10_7
         CGLUpdateContext(g_hw_ctx.CGLContextObj);
         CGLUpdateContext(g_context.CGLContextObj);
#else
         [g_hw_ctx update];
         [g_context update];
#endif
#endif
#endif
         break;
      default:
         break;
   }
}

static void cocoagl_gfx_ctx_destroy(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (!cocoa_ctx)
      return;

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
         [GLContextClass clearCurrentContext];

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
         [g_context clearDrawable];
         RELEASE(g_context);
         RELEASE(g_format);
         if (g_hw_ctx)
         {
            [g_hw_ctx clearDrawable];
         }
         RELEASE(g_hw_ctx);
#endif
         [GLContextClass clearCurrentContext];
         g_context = nil;
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_context_destroy(&cocoa_ctx->vk, cocoa_ctx->vk.vk_surface != VK_NULL_HANDLE);
         if (cocoa_ctx->vk.context.queue_lock) {
            slock_free(cocoa_ctx->vk.context.queue_lock);
         }
         memset(&cocoa_ctx->vk, 0, sizeof(cocoa_ctx->vk));

#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   free(cocoa_ctx);
}

static enum gfx_ctx_api cocoagl_gfx_ctx_get_api(void *data)
{
   return cocoagl_api;
}

static void cocoagl_gfx_ctx_show_mouse(void *data, bool state)
{
   (void)data;

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
   if (state)
      [NSCursor unhide];
   else
      [NSCursor hide];
#endif
}

float cocoagl_gfx_ctx_get_native_scale(void)
{
   static CGFloat ret = 0.0f;
   SEL selector     = NSSelectorFromString(BOXSTRING("nativeScale"));
   RAScreen *screen = (BRIDGE RAScreen*)get_chosen_screen();

   if (ret != 0.0f)
      return ret;
   if (!screen)
      return 0.0f;

   if ([screen respondsToSelector:selector])
      return (float)get_from_selector([screen class], screen, selector, &ret);

   ret          = 1.0f;
   selector     = NSSelectorFromString(BOXSTRING("scale"));
   if ([screen respondsToSelector:selector])
      ret       = screen.scale;
   return ret;
}

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
static void cocoagl_gfx_ctx_update_title(void *data, void *data2)
{
   ui_window_cocoa_t view;
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();

#if defined(HAVE_COCOA)
   view.data                      = (CocoaView*)nsview_get_ptr();
#elif defined(HAVE_COCOA_METAL)
   view.data                      = (BRIDGE void *)apple_platform.renderView;
#endif

   if (window)
   {
      char title[128];

      title[0] = '\0';

      video_driver_get_window_title(title, sizeof(title));

      if (title[0])
         window->set_title(&view, title);
   }
}
#endif

static bool cocoagl_gfx_ctx_get_metrics(void *data, enum display_metric_types type,
            float *value)
{
    RAScreen *screen              = (BRIDGE RAScreen*)get_chosen_screen();
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
    NSDictionary *description     = [screen deviceDescription];
    NSSize  display_pixel_size    = [[description objectForKey:NSDeviceSize] sizeValue];
    CGSize  display_physical_size = CGDisplayScreenSize(
        [[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);

    float   display_width         = display_pixel_size.width;
    float   display_height        = display_pixel_size.height;
    float   physical_width        = display_physical_size.width;
    float   physical_height       = display_physical_size.height;
    float   scale                 = get_backing_scale_factor();
    float   dpi                   = (display_width/ physical_width) * 25.4f * scale;
#elif defined(HAVE_COCOATOUCH)
    float   scale                 = cocoagl_gfx_ctx_get_native_scale();
    CGRect  screen_rect           = [screen bounds];
    float   display_height        = screen_rect.size.height;
    float   physical_width        = screen_rect.size.width  * scale;
    float   physical_height       = screen_rect.size.height * scale;
    float   dpi                   = 160                     * scale;
    unsigned idiom_type           = UI_USER_INTERFACE_IDIOM();

    switch (idiom_type)
    {
       case -1: /* UIUserInterfaceIdiomUnspecified */
          /* TODO */
          break;
       case UIUserInterfaceIdiomPad:
          dpi = 132 * scale;
          break;
       case UIUserInterfaceIdiomPhone:
          dpi = 163 * scale;
          break;
       case UIUserInterfaceIdiomTV:
       case UIUserInterfaceIdiomCarPlay:
          /* TODO */
          break;
    }
#endif

    (void)display_height;

    switch (type)
    {
        case DISPLAY_METRIC_MM_WIDTH:
            *value = physical_width;
            break;
        case DISPLAY_METRIC_MM_HEIGHT:
            *value = physical_height;
            break;
        case DISPLAY_METRIC_DPI:
            *value = dpi;
            break;
        case DISPLAY_METRIC_NONE:
        default:
            *value = 0;
            return false;
    }

    return true;
}

static bool cocoagl_gfx_ctx_has_focus(void *data)
{
   (void)data;
#if defined(HAVE_COCOATOUCH)
    return ([[UIApplication sharedApplication] applicationState] == UIApplicationStateActive);
#else
    return [NSApp isActive];
#endif
}

static bool cocoagl_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

#if !defined(HAVE_COCOATOUCH)
static bool cocoagl_gfx_ctx_has_windowed(void *data)
{
   return true;
}
#endif

static void cocoagl_gfx_ctx_input_driver(void *data,
      const char *name,
      const input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static void cocoagl_gfx_ctx_get_video_size(void *data, unsigned* width, unsigned* height)
{
   float screenscale               = cocoagl_gfx_ctx_get_native_scale();
#if defined(HAVE_COCOA_METAL)
   CGRect size;
   GLsizei backingPixelWidth, backingPixelHeight;
#if defined(HAVE_COCOA_METAL)
   NSView *g_view                  = apple_platform.renderView;
#elif defined(HAVE_COCOA)
   CocoaView *g_view               = (CocoaView*)nsview_get_ptr();
#endif
   CGRect cgrect                   = NSRectToCGRect([g_view frame]);
#if MAC_OS_X_VERSION_10_7
   SEL selector                    = NSSelectorFromString(BOXSTRING("convertRectToBacking:"));
   if ([g_view respondsToSelector:selector])
      cgrect                       = NSRectToCGRect([g_view convertRectToBacking:[g_view bounds]]);
#endif
   backingPixelWidth               = CGRectGetWidth(cgrect);
   backingPixelHeight              = CGRectGetHeight(cgrect);
   size                            = CGRectMake(0, 0, backingPixelWidth, backingPixelHeight);
#else
   CGRect size                     = g_view.bounds;
#endif
   *width                          = CGRectGetWidth(size)  * screenscale;
   *height                         = CGRectGetHeight(size) * screenscale;
}

static gfx_ctx_proc_t cocoagl_gfx_ctx_get_proc_address(const char *symbol_name)
{
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(
               CFBundleGetBundleWithIdentifier(GLFrameworkID),
               (BRIDGE CFStringRef)BOXSTRING(symbol_name)
               );
      case GFX_CTX_NONE:
      default:
         break;
   }

   return NULL;
}

static void cocoagl_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         g_use_hw_ctx = enable;

         if (enable)
            [g_hw_ctx makeCurrentContext];
         else
            [g_context makeCurrentContext];
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void cocoagl_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif

   *quit = false;

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         *resize = cocoa_ctx->vk.need_new_swapchain;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   cocoagl_gfx_ctx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

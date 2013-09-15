/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"

#include "general.h"
#include "gfx/gfx_common.h"
#include "gfx/gfx_context.h"

// Define compatibility symbols and categories
#ifdef IOS
#define APP_HAS_FOCUS ([UIApplication sharedApplication].applicationState == UIApplicationStateActive)

#define GLContextClass EAGLContext
#define GLAPIType GFX_CTX_OPENGL_ES_API
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

@interface EAGLContext (OSXCompat) @end
@implementation EAGLContext (OSXCompat)
+ (void)clearCurrentContext { EAGLContext.currentContext = nil;  }
- (void)makeCurrentContext  { EAGLContext.currentContext = self; }
@end

#elif defined(OSX)
#define APP_HAS_FOCUS ([NSApp isActive])

#define GLContextClass NSOpenGLContext
#define GLAPIType GFX_CTX_OPENGL_API
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen

#define g_view g_instance // < RAGameView is a container on iOS; on OSX these are both the same object

@interface NSScreen (IOSCompat) @end
@implementation NSScreen (IOSCompat)
- (CGRect)bounds { return CGRectMake(0, 0, CGRectGetWidth(self.frame), CGRectGetHeight(self.frame)); }
- (float) scale  { return 1.0f; }
@end

#endif


#ifdef IOS

#import "views.h"
static const float ALMOST_INVISIBLE = .021f;
static GLKView* g_view;
static UIView* g_pause_view;
static UIView* g_pause_indicator_view;

#elif defined(OSX)

#include "apple_input.h"

static bool g_has_went_fullscreen;
static NSOpenGLPixelFormat* g_format;

#endif


static bool g_initialized;
static RAGameView* g_instance;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;


@implementation RAGameView
+ (RAGameView*)get
{
   if (!g_instance)
      g_instance = [RAGameView new];
   
   return g_instance;
}

#ifdef OSX

- (id)init
{
   self = [super init];
   self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
   return self;
}

- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];

   if (g_view && g_context)
      [g_context update];
}

- (void)display
{
   [g_context flushBuffer];
}

// Stop the annoying sound when pressing a key
- (BOOL)acceptsFirstResponder
{
   return YES;
}

- (BOOL)isFlipped
{
   return YES;
}

- (void)keyDown:(NSEvent*)theEvent
{
}

- (void)mouseDown:(NSEvent*)theEvent
{
   g_current_input_data.touch_count = 1;
   [self mouseDragged:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent
{
   g_current_input_data.touch_count = 0;
}

- (void)mouseDragged:(NSEvent*)theEvent
{
   NSPoint pos = [self convertPoint:[theEvent locationInWindow] fromView:nil];
   g_current_input_data.touches[0].screen_x = pos.x;
   g_current_input_data.touches[0].screen_y = pos.y;
}

#elif defined(IOS) // < iOS Pause menu and lifecycle
- (id)init
{
   self = [super init];

   UINib* xib = [UINib nibWithNibName:@"PauseView" bundle:nil];
   g_pause_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
   
   xib = [UINib nibWithNibName:@"PauseIndicatorView" bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];

   g_view = [GLKView new];
   g_view.multipleTouchEnabled = YES;
   g_view.enableSetNeedsDisplay = NO;
   [g_view addSubview:g_pause_view];
   [g_view addSubview:g_pause_indicator_view];

   self.view = g_view;
   return self;
}


// Pause Menus
- (void)viewWillLayoutSubviews
{
   UIInterfaceOrientation orientation = self.interfaceOrientation;
   CGRect screenSize = [[UIScreen mainScreen] bounds];
   
   const float width = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
   const float height = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);

   float tenpctw = width / 10.0f;
   float tenpcth = height / 10.0f;
   
   g_pause_view.frame = CGRectMake(width / 2.0f - 150.0f, height / 2.0f - 150.0f, 300.0f, 300.0f);
   g_pause_indicator_view.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
   [g_pause_indicator_view viewWithTag:1].frame = CGRectMake(0, 0, tenpctw * 2.0f, tenpcth);
}

- (void)openPauseMenu
{
   // Setup save state selector
   UISegmentedControl* stateSelect = (UISegmentedControl*)[g_pause_view viewWithTag:10];
   stateSelect.selectedSegmentIndex = (g_extern.state_slot < 10) ? g_extern.state_slot : -1;

   g_extern.is_paused = true;

   //
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_view.alpha = 1.0f; }
      completion:^(BOOL finished) { }];
}

- (void)closePauseMenu
{
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_view.alpha = 0.0f; }
      completion:^(BOOL finished) { }
   ];
   
   g_extern.is_paused = false;
}

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_indicator_view.alpha = ALMOST_INVISIBLE; }
      completion:^(BOOL finished) { }
   ];
}

#endif

@end

static RAScreen* get_chosen_screen()
{
   unsigned monitor = g_settings.video.monitor_index;
         
   if (monitor >= RAScreen.screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.\n");
      return RAScreen.mainScreen;
   }
   
   return RAScreen.screens[monitor];
}

bool apple_gfx_ctx_init()
{
   dispatch_sync(dispatch_get_main_queue(),
   ^{
      // Make sure the view was created
      [RAGameView get];      
      
#ifdef IOS // Show pause button for a few seconds, so people know it's there
      g_pause_indicator_view.alpha = 1.0f;
      [NSObject cancelPreviousPerformRequestsWithTarget:g_instance];
      [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
#endif
   });

   g_initialized = true;

   return true;
}

void apple_gfx_ctx_destroy()
{
   g_initialized = false;

   [GLContextClass clearCurrentContext];

   dispatch_sync(dispatch_get_main_queue(),
   ^{
#ifdef IOS
      g_view.context = nil;
#endif
      [GLContextClass clearCurrentContext];
      g_context = nil;
   });
}

bool apple_gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api != GLAPIType)
      return false;


   [GLContextClass clearCurrentContext];

   dispatch_sync(dispatch_get_main_queue(),
   ^{
      [GLContextClass clearCurrentContext];
   
#ifdef OSX
      [g_context clearDrawable];
      g_context = nil;
      g_format = nil;
   
      NSOpenGLPixelFormatAttribute attributes [] = {
         NSOpenGLPFADoubleBuffer,	// double buffered
         NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
         (major || minor) ? NSOpenGLPFAOpenGLProfile : 0, (major << 12) | (minor << 8),
         (NSOpenGLPixelFormatAttribute)nil
      };

      g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
      g_context.view = g_view;
#else
      g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
      g_view.context = g_context;
#endif

      [g_context makeCurrentContext];
   });
   
   [g_context makeCurrentContext];
   return true;
}

void apple_gfx_ctx_swap_interval(unsigned interval)
{
#ifdef IOS // < No way to disable Vsync on iOS?
           //   Just skip presents so fast forward still works.
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(OSX)
   GLint value = interval ? 1 : 0;
   [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];
#endif
}

bool apple_gfx_ctx_set_video_mode(unsigned width, unsigned height, bool fullscreen)
{
#ifdef OSX
   dispatch_sync(dispatch_get_main_queue(),
   ^{
      // TODO: Sceen mode support
      
      if (fullscreen && !g_has_went_fullscreen)
      {
         [g_view enterFullScreenMode:get_chosen_screen() withOptions:nil];
         [NSCursor hide];
      }
      else if (!fullscreen && g_has_went_fullscreen)
      {
         [g_view exitFullScreenModeWithOptions:nil];
         [g_view.window makeFirstResponder:g_view];
         [NSCursor unhide];
      }
      
      g_has_went_fullscreen = fullscreen;
      if (!g_has_went_fullscreen)
         [g_view.window setContentSize:NSMakeSize(width, height)];
   });
#endif

   // TODO: Maybe iOS users should be apple to show/hide the status bar here?

   return true;
}

void apple_gfx_ctx_get_video_size(unsigned* width, unsigned* height)
{
   RAScreen* screen = get_chosen_screen();
   CGRect size = g_initialized ? g_view.bounds : screen.bounds;

   *width  = CGRectGetWidth(size)  * screen.scale;
   *height = CGRectGetHeight(size) * screen.scale;
}

void apple_gfx_ctx_update_window_title(void)
{
#ifdef OSX
   static char buf[128];
   bool got_text = gfx_get_fps(buf, sizeof(buf), false);
   static const char* const text = buf; // < Can't access buf directly in the block
   
   if (got_text)
   {
      // NOTE: This could go bad if buf is updated again before this completes.
      //       If it poses a problem it should be changed to dispatch_sync.
      dispatch_async(dispatch_get_main_queue(),
      ^{
         g_view.window.title = @(text);
      });
   }
#endif
}

bool apple_gfx_ctx_has_focus(void)
{
   return APP_HAS_FOCUS;
}

void apple_gfx_ctx_swap_buffers()
{
   if (--g_fast_forward_skips < 0)
   {
      dispatch_sync(dispatch_get_main_queue(),
      ^{
         [g_view display];
      });

      g_fast_forward_skips = g_is_syncing ? 0 : 3;
   }
}

gfx_ctx_proc_t apple_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
                                                            (__bridge CFStringRef)(@(symbol_name)));
}

#ifdef IOS
void apple_bind_game_view_fbo(void)
{
   dispatch_sync(dispatch_get_main_queue(), ^{
      if (g_context)
         [g_view bindDrawable];
   });
}
#endif

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

#ifdef IOS

#import "views.h"

static const float ALMOST_INVISIBLE = .021f;
static RAGameView* g_instance;
static GLKView* g_view;
static EAGLContext* g_context;
static UIView* g_pause_view;
static UIView* g_pause_indicator_view;

#elif defined(OSX)

static RAGameView* g_instance;
static NSOpenGLContext* g_context;

#define g_view g_instance // < RAGameView is a container on iOS; on OSX these are both the same object

#endif

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static float g_screen_scale = 1.0f;

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
   static const NSOpenGLPixelFormatAttribute attributes [] = {
      NSOpenGLPFAWindow,
      NSOpenGLPFADoubleBuffer,	// double buffered
      NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
      (NSOpenGLPixelFormatAttribute)nil
   };

   self = [super initWithFrame:CGRectMake(0, 0, 100, 100) pixelFormat:[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes]];
   self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;   
   
   g_context = self.openGLContext;
   [g_context makeCurrentContext];
   
   return self;
}

- (void)display
{
   [self.openGLContext flushBuffer];
}

#elif defined(IOS) // < iOS Pause menu and lifecycle
- (id)init
{
   self = [super init];

   g_screen_scale = [[UIScreen mainScreen] scale];

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

- (void)suspend
{
   g_view.context = nil;
   [EAGLContext setCurrentContext:nil];
}

- (void)resume
{
   g_view.context = g_context;
   [EAGLContext setCurrentContext:g_context];
}
#endif

@end

// Realistically these functions don't create or destory the view; just the OpenGL context.
bool apple_init_game_view()
{
#ifdef IOS
   dispatch_sync(dispatch_get_main_queue(), ^{
      // Make sure the view was created
      [RAGameView get];

      g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
      [EAGLContext setCurrentContext:g_context];
      g_view.context = g_context;

      // Show pause button for a few seconds, so people know it's there
      g_pause_indicator_view.alpha = 1.0f;
      [NSObject cancelPreviousPerformRequestsWithTarget:g_instance];
      [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
   });

   [EAGLContext setCurrentContext:g_context];
#endif
   return true;
}

void apple_destroy_game_view()
{
#ifdef IOS
   dispatch_sync(dispatch_get_main_queue(), ^{
      // Clear the view, otherwise the last frame form this game will be displayed
      // briefly on the next game.
      [g_view bindDrawable];
      glClear(GL_COLOR_BUFFER_BIT);
      [g_view display];
   
      glFinish();

      g_view.context = nil;
      [EAGLContext setCurrentContext:nil];
      g_context = nil;
   });
   
   [EAGLContext setCurrentContext:nil];
#endif
}

void apple_flip_game_view()
{
   if (--g_fast_forward_skips < 0)
   {
      dispatch_sync(dispatch_get_main_queue(), ^{
         [g_view display];
      });
      g_fast_forward_skips = g_is_syncing ? 0 : 3;
   }
}

void apple_set_game_view_sync(unsigned interval)
{
#ifdef IOS // < No way to disable Vsync on iOS?
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(OSX)
   GLint value = interval ? 0 : 1;
   [g_view.openGLContext setValues:&value forParameter:NSOpenGLCPSwapInterval];
#endif
}

void apple_get_game_view_size(unsigned *width, unsigned *height)
{
   *width  = g_view.bounds.size.width * g_screen_scale;
   *width = *width ? *width : 640;
   
   *height = g_view.bounds.size.height * g_screen_scale;
   *height = *height ? *height : 480;
}

void apple_bind_game_view_fbo(void)
{
#ifdef IOS
   dispatch_sync(dispatch_get_main_queue(), ^{
      if (g_context)
         [g_view bindDrawable];
   });
#else
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

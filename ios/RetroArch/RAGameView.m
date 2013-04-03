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

#include "general.h"
#include "rarch_wrapper.h"

static const float ALMOST_INVISIBLE = .021f;
static float g_screen_scale;
static int g_fast_forward_skips;
static bool g_is_syncing = true;
static RAGameView* g_instance;
static GLKView* g_view;
static EAGLContext* g_context;
static UIView* g_pause_view;;
static UIView* g_pause_indicator_view;

@implementation RAGameView
+ (RAGameView*)get
{
   if (!g_instance)
      g_instance = [RAGameView new];
   
   return g_instance;
}

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
}

- (void)openPauseMenu
{
   // Setup save state selector
   UISegmentedControl* stateSelect = (UISegmentedControl*)[g_pause_view viewWithTag:1];
   stateSelect.selectedSegmentIndex = (g_extern.state_slot < 10) ? g_extern.state_slot : -1;

   g_extern.is_paused = true;

   //
   [UIView animateWithDuration:0.2
      animations:^ { g_pause_view.alpha = 1.0f; }
      completion:^(BOOL finished){}];
}

- (void)closePauseMenu
{
   [UIView animateWithDuration:0.2
      animations:^ { g_pause_view.alpha = 0.0f; }
      completion:^(BOOL finished) { }
   ];
   
   g_extern.is_paused = false;
}

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^ { g_pause_indicator_view.alpha = ALMOST_INVISIBLE; }
         completion:^(BOOL finished) { }
      ];
}

@end

bool ios_init_game_view()
{
   dispatch_sync(dispatch_get_main_queue(), ^{
      // Make sure the view was created
      [RAGameView get];

      g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
      [EAGLContext setCurrentContext:g_context];
      g_view.context = g_context;

      // Show pause button for a few seconds, so people know it's there
      g_pause_indicator_view.alpha = 1.0f;
      [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
   });

   [EAGLContext setCurrentContext:g_context];

   return true;
}

void ios_destroy_game_view()
{
   dispatch_sync(dispatch_get_main_queue(), ^{
      glFinish();

      g_view.context = nil;
      [EAGLContext setCurrentContext:nil];
      g_context = nil;
   });
   
   [EAGLContext setCurrentContext:nil];
}

void ios_flip_game_view()
{
   if (--g_fast_forward_skips < 0)
   {
      dispatch_sync(dispatch_get_main_queue(), ^{
         [g_view display];
      });
      g_fast_forward_skips = g_is_syncing ? 0 : 3;
   }
}

void ios_set_game_view_sync(bool on)
{
   g_is_syncing = on;
   g_fast_forward_skips = on ? 0 : 3;
}

void ios_get_game_view_size(unsigned *width, unsigned *height)
{
   *width  = g_view.bounds.size.width * g_screen_scale;
   *width = *width ? *width : 640;
   
   *height = g_view.bounds.size.height * g_screen_scale;
   *height = *height ? *height : 480;
}

void ios_bind_game_view_fbo()
{
   dispatch_sync(dispatch_get_main_queue(), ^{
      [g_view bindDrawable];   
   });
}


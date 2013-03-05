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
static int g_frame_skips = 4;
static bool g_is_syncing = true;
static RAGameView* g_instance;

@implementation RAGameView
{
   EAGLContext* _glContext;

   UIView* _pauseView;
   UIView* _pauseIndicatorView;
}

+ (RAGameView*)get
{
   if (!g_instance)
      g_instance = [RAGameView new];
   
   return g_instance;
}

- (id)init
{
   self = [super init];

   UINib* xib = [UINib nibWithNibName:@"PauseView" bundle:nil];
   _pauseView = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
   
   xib = [UINib nibWithNibName:@"PauseIndicatorView" bundle:nil];
   _pauseIndicatorView = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];

   self.view = [GLKView new];
   self.view.multipleTouchEnabled = YES;
   [self.view addSubview:_pauseView];
   [self.view addSubview:_pauseIndicatorView];

   return self;
}

// Driver
- (void)driverInit
{
   g_screen_scale = [[UIScreen mainScreen] scale];

   _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   [EAGLContext setCurrentContext:_glContext];
   ((GLKView*)self.view).context = _glContext;

   // Show pause button for a few seconds, so people know it's there
   _pauseIndicatorView.alpha = 1.0f;
   [self performSelector:@selector(hidePauseButton) withObject:self afterDelay:3.0f];
}

- (void)driverQuit
{
   glFinish();

   ((GLKView*)self.view).context = nil;
   [EAGLContext setCurrentContext:nil];
   _glContext = nil;
}

- (void)flip
{
   if (--g_frame_skips < 0)
   {
      [self.view setNeedsDisplay];
      [(GLKView*)self.view bindDrawable];
      g_frame_skips = g_is_syncing ? 0 : 3;
   }
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
   
   _pauseView.frame = CGRectMake(width / 2.0f - 150.0f, height / 2.0f - 150.0f, 300.0f, 300.0f);
   _pauseIndicatorView.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
}

- (void)openPauseMenu
{
   // Setup save state selector
   UISegmentedControl* stateSelect = (UISegmentedControl*)[_pauseView viewWithTag:1];
   stateSelect.selectedSegmentIndex = (g_extern.state_slot < 10) ? g_extern.state_slot : -1;

   //
   [UIView animateWithDuration:0.2
      animations:^ { _pauseView.alpha = 1.0f; }
      completion:^(BOOL finished){}];
}

- (void)closePauseMenu
{
   [UIView animateWithDuration:0.2
      animations:^ { _pauseView.alpha = 0.0f; }
      completion:^(BOOL finished) { }
   ];
}

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^ { _pauseIndicatorView.alpha = ALMOST_INVISIBLE; }
         completion:^(BOOL finished) { }
      ];
}

@end

bool ios_init_game_view()
{
   [RAGameView.get driverInit];
   return true;
}

void ios_destroy_game_view()
{
   [RAGameView.get driverQuit];
}

void ios_flip_game_view()
{
   [RAGameView.get flip];
}

void ios_set_game_view_sync(bool on)
{
   g_is_syncing = on;
   g_frame_skips = on ? 0 : 3;
}

void ios_get_game_view_size(unsigned *width, unsigned *height)
{
   *width  = RAGameView.get.view.bounds.size.width * g_screen_scale;
   *width = *width ? *width : 640;
   
   *height = RAGameView.get.view.bounds.size.height * g_screen_scale;
   *height = *height ? *height : 480;
}

void ios_bind_game_view_fbo()
{
   [(GLKView*)RAGameView.get.view bindDrawable];
}

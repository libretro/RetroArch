/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

static bool _isRunning;
static bool _isPaused;

static float screen_scale;
static int frame_skips = 4;
static bool is_syncing = true;

@implementation RAGameView
{
   EAGLContext* _glContext;
   UIButton* _notifyButton;
   UILabel* _notifyLabel;
}

- (id)init
{
   self = [super init];
   screen_scale = [[UIScreen mainScreen] scale];
   
   _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   [EAGLContext setCurrentContext:_glContext];
   
   self.view = [GLKView new];
   ((GLKView*)self.view).context = _glContext;
   self.view.multipleTouchEnabled = YES;

   return self;
}

- (void)viewDidAppear:(BOOL)animated
{
   CGSize size = self.view.bounds.size;
   float tenpct = size.width / 10.0f;
   
   _notifyButton = [[UIButton alloc] initWithFrame:CGRectMake(tenpct * 4.0f, 0, tenpct * 2.0f, size.height / 10.0f)];
   _notifyButton.backgroundColor = [UIColor redColor];
   _notifyButton.opaque = NO;
   _notifyButton.userInteractionEnabled = NO;

   _notifyLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height / 10.0f)];
   _notifyLabel.backgroundColor = [UIColor colorWithRed:0.2f green:0.2f blue: 0.5f alpha:0.5f];
   _notifyLabel.text = @"Triple tap to exit.";
   _notifyLabel.textAlignment = NSTextAlignmentCenter;
   _notifyLabel.opaque = NO;
   _notifyLabel.userInteractionEnabled = NO;
  
   [self.view addSubview:_notifyButton];
   [self.view addSubview:_notifyLabel];
   [self performSelector:@selector(hideNotify) withObject:nil afterDelay:3.0f];
}

- (void)hideNotify
{
   if (_notifyLabel && _notifyButton)
   {
      // TODO: Actually removing these views will cause an ugly flash in the game window...
      [UIView animateWithDuration:0.2
         animations:^{_notifyButton.alpha = 0.0;}
         completion:^(BOOL finished){ _notifyButton.hidden = YES; _notifyButton = nil; }];

      [UIView animateWithDuration:0.2
         animations:^{_notifyLabel.alpha = 0.0;}
         completion:^(BOOL finished){ _notifyLabel.hidden = YES; _notifyLabel = nil; }];
   }
}

- (void)iterate
{
   while (_isRunning && !_isPaused)
   {
      _isRunning = rarch_main_iterate();

      if (!_isRunning)
      {
         ios_close_game();
         return;
      }
      else
         while(!_isPaused && _isRunning && CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource);
   }
}

- (void)needsToDie
{
   glFinish();

   GLKView* glview = (GLKView*)self.view;
   glview.context = nil;
   _glContext = nil;
   [EAGLContext setCurrentContext:nil];
}

- (void)pause
{
   _isPaused = true;
}

- (void)resume
{
   if (_isPaused)
   {
      _isPaused = false;
      [self performSelector:@selector(iterate) withObject:nil afterDelay:.02f];
   }
}

@end

static RAGameView* gameViewer;

bool ios_load_game(const char* path)
{
   [RASettingsList refreshConfigFile];
   
   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf =[[RetroArch_iOS get].config_file_path UTF8String];
   const char* const libretro = [[RetroArch_iOS get].module_path UTF8String];

   struct rarch_main_wrap main_wrapper = {path, sd, sd, cf, libretro};
   if (rarch_main_init_wrap(&main_wrapper) == 0)
   {
      rarch_init_msg_queue();
      _isRunning = true;
   }
   else
      _isRunning = false;
   
   return _isRunning;
}

void ios_close_game()
{
   if (_isRunning)
   {
      rarch_main_deinit();
      rarch_deinit_msg_queue();

#ifdef PERF_TEST
      rarch_perf_log();
#endif

      rarch_main_clear_state();
      
      _isRunning = false;
   }
   
   [[RetroArch_iOS get] gameHasExited];
}

void ios_pause_emulator()
{
   if (_isRunning)
      [gameViewer pause];
}

void ios_resume_emulator()
{
   if (_isRunning)
      [gameViewer resume];
}

void ios_suspend_emulator()
{
   if (_isRunning)
      uninit_drivers();
}

void ios_activate_emulator()
{
   if (_isRunning)
      init_drivers();
}

bool ios_init_game_view()
{
   if (!gameViewer)
   {
      gameViewer = [RAGameView new];
      [[RetroArch_iOS get] setViewer:gameViewer];
      [gameViewer performSelector:@selector(iterate) withObject:nil afterDelay:.02f];
   }
   
   return true;
}

void ios_destroy_game_view()
{
   if (gameViewer)
   {
      [gameViewer needsToDie];
      [[RetroArch_iOS get] setViewer:nil];
      gameViewer = nil;
   }
}

void ios_flip_game_view()
{
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
   
      if (--frame_skips < 0)
      {
         [gl_view setNeedsDisplay];
         [gl_view bindDrawable];
         frame_skips = is_syncing ? 0 : 3;
      }
   }
}

void ios_set_game_view_sync(bool on)
{
   is_syncing = on;
   frame_skips = on ? 0 : 3;
}

void ios_get_game_view_size(unsigned *width, unsigned *height)
{
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
   
      *width  = gl_view.bounds.size.width * screen_scale;
      *height = gl_view.bounds.size.height * screen_scale;
   }
}

void ios_bind_game_view_fbo()
{
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
      [gl_view bindDrawable];
   }
}

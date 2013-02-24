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

#include <sys/stat.h>
#include "rarch_wrapper.h"
#include "general.h"

@interface RANavigator : UINavigationController
// 0 if no RAGameView is in the navigator
// 1 if a RAGameView is the top
// 2+ if there are views pushed ontop of the RAGameView
@property unsigned gameAndAbove;

- (void)pushViewController:(UIViewController*)theView isGame:(BOOL)game;
@end

@implementation RANavigator
- (void)pushViewController:(UIViewController*)theView isGame:(BOOL)game
{
   assert(!game || self.gameAndAbove == 0);

   if (game || self.gameAndAbove) self.gameAndAbove ++;
   
   [[UIApplication sharedApplication] setStatusBarHidden:game withAnimation:UIStatusBarAnimationNone];
   self.navigationBarHidden = game;
   [self pushViewController:theView animated:!(self.gameAndAbove == 1 || self.gameAndAbove == 2)];
}

- (UIViewController *)popViewControllerAnimated:(BOOL)animated
{
   const bool poppingFromGame = self.gameAndAbove == 1;
   const bool poppingToGame = self.gameAndAbove == 2;
   if (self.gameAndAbove) self.gameAndAbove --;
   
   if (self.gameAndAbove == 1)
      [[RetroArch_iOS get] performSelector:@selector(startTimer)];

   [[UIApplication sharedApplication] setStatusBarHidden:poppingToGame withAnimation:UIStatusBarAnimationNone];
   self.navigationBarHidden = poppingToGame;
   return [super popViewControllerAnimated:!poppingToGame && !poppingFromGame];
}
@end

@implementation RetroArch_iOS
{
   UIWindow* _window;
   RANavigator* _navigator;
   NSTimer* _gameTimer;
   UIView* _pauseView;
   
   bool _isPaused;
   bool _isRunning;
}

+ (void)displayErrorMessage:(NSString*)message
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                             message:message
                                             delegate:nil
                                             cancelButtonTitle:@"OK"
                                             otherButtonTitles:nil];
   [alert show];
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)showSettings
{
   [self pushViewController:[RASettingsList new] isGame:NO];
}

- (NSString*)configFilePath
{
   if (self.module_path)
   {
      return [NSString stringWithFormat:@"%@/%@.cfg", self.system_directory, [[self.module_path lastPathComponent] stringByDeletingPathExtension]];
   }
   
   return nil;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   // TODO: Relocate this!
   self.system_directory = @"/var/mobile/Library/RetroArch/";
   mkdir([self.system_directory UTF8String], 0755);
         
   // Load icons
   self.file_icon = [UIImage imageNamed:@"ic_file"];
   self.folder_icon = [UIImage imageNamed:@"ic_dir"];

   // Load buttons
   self.settings_button = [[UIBarButtonItem alloc]
                          initWithTitle:@"Module Settings"
                          style:UIBarButtonItemStyleBordered
                          target:nil action:nil];
   self.settings_button.target = self;
   self.settings_button.action = @selector(showSettings);

   // Load pause menu
   UINib* xib = [UINib nibWithNibName:@"PauseView" bundle:nil];
   _pauseView = [[xib instantiateWithOwner:self options:nil] lastObject];
   _pauseView.opaque = NO;
   _pauseView.alpha = 0.0f;

   [[UIApplication sharedApplication] setStatusBarHidden:NO withAnimation:UIStatusBarAnimationNone];

   // Setup window
   _navigator = [RANavigator new];
   [_navigator pushViewController: [RAModuleList new] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = _navigator;
   [_window makeKeyAndVisible];
   
   // Setup keyboard hack
}

#pragma mark VIEW MANAGEMENT
- (void)pushViewController:(UIViewController*)theView isGame:(BOOL)game
{
   if (game)
      [theView.view addSubview:_pauseView];

   [_navigator pushViewController:theView isGame:game];
   [self startTimer];
}

- (void)popViewController
{
   if (_navigator.gameAndAbove == 1)
      [_pauseView removeFromSuperview];

   [_navigator popViewControllerAnimated:YES];
}

#pragma mark EMULATION
- (void)runGame:(NSString*)path
{
   [RASettingsList refreshConfigFile];
   
   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf =[[RetroArch_iOS get].configFilePath UTF8String];
   const char* const libretro = [[RetroArch_iOS get].module_path UTF8String];

   struct rarch_main_wrap main_wrapper = {[path UTF8String], sd, sd, cf, libretro};
   if (rarch_main_init_wrap(&main_wrapper) == 0)
   {
      _isRunning = true;
      rarch_init_msg_queue();
      [self startTimer];
   }
   else
   {
      _isRunning = false;
      [RetroArch_iOS displayErrorMessage:@"Failed to load game."];
   }
}

- (void)closeGame
{
   if (_isRunning)
   {
      rarch_main_deinit();
      rarch_deinit_msg_queue();

#ifdef PERF_TEST
      rarch_perf_log();
#endif

      rarch_main_clear_state();
   }
   
   [self stopTimer];
   _isRunning = false;
}

- (void)iterate
{
   if (_isPaused || !_isRunning || _navigator.gameAndAbove != 1)
      [self stopTimer];
   else if (_isRunning && !rarch_main_iterate())
      [self closeGame];
}

- (void)startTimer
{
   if (!_gameTimer)
      _gameTimer = [NSTimer scheduledTimerWithTimeInterval:0.001f target:self selector:@selector(iterate) userInfo:nil repeats:YES];
}

- (void)stopTimer
{
   if (_gameTimer)
      [_gameTimer invalidate];
   
   _gameTimer = nil;
}

#pragma mark LIFE CYCLE
- (void)applicationDidBecomeActive:(UIApplication*)application
{
   [self startTimer];
}

- (void)applicationWillResignActive:(UIApplication*)application
{
   [self stopTimer];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
   if (_isRunning)
      init_drivers();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   if (_isRunning)
      uninit_drivers();
}

#pragma mark INPUT
- (void)touchesBegan:(NSSet*)theTouches withEvent:(UIEvent *)event
{
   NSArray* touches = [theTouches allObjects];
   UIView* view = _window.rootViewController.view;
   
   const int count = [touches count];
   for(int i = 0; i != count; i ++)
   {
      UITouch* touch = [touches objectAtIndex:i];
      CGPoint coord = [touch locationInView:view];
      
      // Exit hack!
      if (!_isPaused && _navigator.gameAndAbove == 1 && touch.tapCount == 3)
      {
         if (coord.y < view.bounds.size.height / 10.0f)
         {
            float tenpct = view.bounds.size.width / 10.0f;
            if (coord.x >= tenpct * 4 && coord.x <= tenpct * 6)
            {
               _isPaused = true;
               _pauseView.frame = CGRectMake(view.bounds.size.width / 2.0f - 100.0f, view.bounds.size.height / 2.0f - 100.0f, 200.0f, 200.0f);
               
               [UIView animateWithDuration:0.2
                  animations:^{_pauseView.alpha = 1.0f;}
                  completion:^(BOOL finished){}];
            }
         }
      }
   }
}

#pragma mark PAUSE MENU
- (IBAction)closeGamePressed:(id)sender
{
   [self closeGame];
   _pauseView = nil;
}

- (IBAction)resumeGamePressed:(id)sender
{
   if (_isPaused)
      [UIView animateWithDuration:0.2
         animations:^{_pauseView.alpha = 0.0;}
         completion:^(BOOL finished)
         {
            _isPaused = false;
            [self startTimer];
         }
      ];
}

@end


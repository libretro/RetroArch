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

#define ALMOST_INVISIBLE .021f

@interface RANavigator : UINavigationController
@end

@implementation RANavigator
{
   RetroArch_iOS* _delegate;
}

- (id)initWithAppDelegate:(RetroArch_iOS*)delegate
{
   self = [super init];

   assert(delegate);
   _delegate = delegate;
   
   return self;
}

- (UIViewController *)popViewControllerAnimated:(BOOL)animated
{
   return [_delegate popViewController];
}

- (UIViewController*)reallyPopViewControllerAnimated:(BOOL)animated
{
   return [super popViewControllerAnimated:animated];
}

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
   [_delegate performSelector:@selector(screenDidRotate) withObject:nil afterDelay:.01f];
}

@end

@implementation RetroArch_iOS
{
   UIWindow* _window;
   RANavigator* _navigator;
   NSTimer* _gameTimer;

   UIView* _pauseView;
   UIView* _pauseIndicatorView;
   RAGameView* _game;
   
   bool _isPaused;
   bool _isRunning;
   
   // 0 if no RAGameView is in the navigator
   // 1 if a RAGameView is the top
   // 2+ if there are views pushed ontop of the RAGameView
   unsigned _gameAndAbove;

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
   
   xib = [UINib nibWithNibName:@"PauseIndicatorView" bundle:nil];
   _pauseIndicatorView = [[xib instantiateWithOwner:self options:nil] lastObject];

   // Show status bar
   [[UIApplication sharedApplication] setStatusBarHidden:NO withAnimation:UIStatusBarAnimationNone];

   // Setup window
   _navigator = [[RANavigator alloc] initWithAppDelegate:self];
   [_navigator pushViewController: [RAModuleList new] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = _navigator;
   [_window makeKeyAndVisible];
}

#pragma mark VIEW MANAGEMENT
- (void)screenDidRotate
{
   UIInterfaceOrientation orientation = _navigator.interfaceOrientation;
   CGRect screenSize = [[UIScreen mainScreen] bounds];
   
   const float width = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
   const float height = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);

   float tenpctw = width / 10.0f;
   float tenpcth = height / 10.0f;
   
   _pauseView.frame = CGRectMake(width / 2.0f - 150.0f, height / 2.0f - 150.0f, 300.0f, 300.0f);
   _pauseIndicatorView.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
}

- (void)pushViewController:(UIViewController*)theView isGame:(BOOL)game
{
   assert(!game || _gameAndAbove == 0);

   _gameAndAbove += (game || _gameAndAbove) ? 1 : 0;

   // Update status and navigation bars
   [[UIApplication sharedApplication] setStatusBarHidden:game withAnimation:UIStatusBarAnimationNone];
   _navigator.navigationBarHidden = game;
   
   //
   [_navigator pushViewController:theView animated:!(_gameAndAbove == 1 || _gameAndAbove == 2)];
   
   if (game)
   {
      _game = (RAGameView*)theView;
   
      _pauseIndicatorView.alpha = ALMOST_INVISIBLE;
      _pauseIndicatorView.userInteractionEnabled = YES;

      [theView.view addSubview:_pauseView];
      [theView.view addSubview:_pauseIndicatorView];

      [self startTimer];
      [self performSelector:@selector(screenDidRotate) withObject:nil afterDelay:.01f];
   }
}

- (UIViewController*)popViewController
{
   const bool poppingFromGame = _gameAndAbove == 1;
   const bool poppingToGame = _gameAndAbove == 2;
   
   _gameAndAbove -= (_gameAndAbove) ? 1 : 0;
   
   if (poppingToGame)
      [self startTimer];

   // Update status and navigation bar
   [[UIApplication sharedApplication] setStatusBarHidden:poppingToGame withAnimation:UIStatusBarAnimationNone];
   _navigator.navigationBarHidden = poppingToGame;
   
   //
   if (poppingFromGame)
   {
      [_pauseView removeFromSuperview];
      [_pauseIndicatorView removeFromSuperview];
   }
   
   return [_navigator reallyPopViewControllerAnimated:!poppingToGame && !poppingFromGame];
}

#pragma mark EMULATION
- (void)runGame:(NSString*)path
{
   [RASettingsList refreshConfigFile];
   
   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf =[[RetroArch_iOS get].moduleInfo.configPath UTF8String];
   const char* const libretro = [[RetroArch_iOS get].moduleInfo.path UTF8String];

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
   if (_isPaused || !_isRunning || _gameAndAbove != 1)
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

#pragma mark PAUSE MENU
- (IBAction)showPauseMenu:(id)sender
{
   if (_isRunning && !_isPaused && _gameAndAbove == 1)
   {
      _isPaused = true;

      UISegmentedControl* stateSelect = (UISegmentedControl*)[_pauseView viewWithTag:1];
      stateSelect.selectedSegmentIndex = (g_extern.state_slot < 10) ? g_extern.state_slot : -1;
      
      [UIView animateWithDuration:0.2
         animations:^
         {
            _pauseIndicatorView.alpha = ALMOST_INVISIBLE;
            _pauseView.alpha = 1.0f;
         }
         completion:^(BOOL finished){}];
   }
}

- (IBAction)resetGame:(id)sender
{
   if (_isRunning) rarch_game_reset();
   [self closePauseMenu:sender];
}

- (IBAction)loadState:(id)sender
{
   if (_isRunning) rarch_load_state();
   [self closePauseMenu:sender];
}

- (IBAction)saveState:(id)sender
{
   if (_isRunning) rarch_save_state();
   [self closePauseMenu:sender];
}

- (IBAction)chooseState:(id)sender
{
   g_extern.state_slot = ((UISegmentedControl*)sender).selectedSegmentIndex;
}

- (IBAction)closePauseMenu:(id)sender
{
   if (_isPaused)
      [UIView animateWithDuration:0.2 
         animations:^
         {
            _pauseView.alpha = 0.0f;
            _pauseIndicatorView.alpha = ALMOST_INVISIBLE;
         }
         completion:^(BOOL finished)
         {
            _isPaused = false;
            [self startTimer];
         }
      ];
}

- (IBAction)closeGamePressed:(id)sender
{
   [self closePauseMenu:sender];
   [self closeGame];
}

- (IBAction)showSettings
{
   [self pushViewController:[RASettingsList new] isGame:NO];
}


@end


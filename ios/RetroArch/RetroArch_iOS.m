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
#import "browser/browser.h"
#import "settings/settings.h"

#import "input/BTStack/WiiMoteHelper.h"

@implementation RetroArch_iOS
{
   UIWindow* _window;
   NSTimer* _gameTimer;
   
   bool _isGameTop;
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

// UIApplicationDelegate
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   // TODO: Relocate this!
   self.system_directory = @"/var/mobile/Library/RetroArch/";
   mkdir([self.system_directory UTF8String], 0755);
         
   // Setup window
   self.delegate = self;
   [self pushViewController:[RADirectoryList directoryListOrGridWithPath:nil] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = self;
   [_window makeKeyAndVisible];
}

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

// UINavigationControllerDelegate
- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   _isGameTop = [viewController isKindOfClass:[RAGameView class]];
   [[UIApplication sharedApplication] setStatusBarHidden:_isGameTop withAnimation:UIStatusBarAnimationNone];
   self.navigationBarHidden = _isGameTop;
   
   if (_isGameTop)
      [self startTimer];

   self.topViewController.navigationItem.rightBarButtonItem = [self createBluetoothButton];
}

// UINavigationController: Never animate when pushing onto, or popping, an RAGameView
- (void)pushViewController:(UIViewController*)theView animated:(BOOL)animated
{
   [super pushViewController:theView animated:animated && !_isGameTop];
}

- (UIViewController*)popViewControllerAnimated:(BOOL)animated
{
   return [super popViewControllerAnimated:animated && !_isGameTop];
}

#pragma mark EMULATION
- (void)runGame:(NSString*)path
{
   assert(self.moduleInfo);
   
   [RASettingsList refreshConfigFile];

   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf = (ra_ios_is_file(self.moduleInfo.configPath)) ? [self.moduleInfo.configPath UTF8String] : 0;
   const char* const libretro = [self.moduleInfo.path UTF8String];

   struct rarch_main_wrap main_wrapper = {[path UTF8String], sd, sd, cf, libretro};
   if (rarch_main_init_wrap(&main_wrapper) == 0)
   {
      rarch_init_msg_queue();

      // Read load time settings
      RAConfig* conf = [[RAConfig alloc] initWithPath:self.moduleInfo.configPath];
      if ([conf getBoolNamed:@"ios_auto_bluetooth" withDefault:false])
         [self startBluetooth];

      //
      [self pushViewController:RAGameView.get animated:NO];
      _isRunning = true;
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
      _isRunning = false;
   
      rarch_main_deinit();
      rarch_deinit_msg_queue();
      rarch_main_clear_state();
      
      // Stop bluetooth (might be annoying but forgetting could eat battery of device AND wiimote)
      [self stopBluetooth];
      
      //
      [self popToViewController:[RAGameView get] animated:NO];
      [self popViewControllerAnimated:NO];
   }
}

- (void)refreshConfig
{
   // Need to clear these otherwise stale versions may be used!
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.bsnes_shader_path, 0, sizeof(g_settings.video.bsnes_shader_path));

   if (_isRunning)
   {
      uninit_drivers();
      config_load();
      init_drivers();
   }
}

- (void)iterate
{
   if (_isPaused || !_isRunning || !_isGameTop)
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

#pragma mark PAUSE MENU
- (IBAction)showPauseMenu:(id)sender
{
   if (_isRunning && !_isPaused && _isGameTop)
   {
      _isPaused = true;
      [[RAGameView get] openPauseMenu];
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
   [[RAGameView get] closePauseMenu];

   if (_isPaused)
   {
      _isPaused = false;
      [self startTimer];
   }
}

- (IBAction)closeGamePressed:(id)sender
{
   [self closePauseMenu:sender];
   [self closeGame];
}

- (IBAction)showSettings
{
   [self pushViewController:[RASettingsList new] animated:YES];
}

#pragma mark Bluetooth Helpers
- (UIBarButtonItem*)createBluetoothButton
{
   if ([WiiMoteHelper haveBluetooth])
   {
      const bool isBTOn = [WiiMoteHelper isBluetoothRunning];
      return [[UIBarButtonItem alloc]
               initWithTitle:isBTOn ? @"Stop Bluetooth" : @"Start Bluetooth"
               style:UIBarButtonItemStyleBordered
               target:[RetroArch_iOS get]
               action:isBTOn ? @selector(stopBluetooth) : @selector(startBluetooth)];
   }
   else
      return nil;
}

- (IBAction)startBluetooth
{
   if ([WiiMoteHelper haveBluetooth])
   {
      [WiiMoteHelper startBluetooth];
      [self.topViewController.navigationItem setRightBarButtonItem:[self createBluetoothButton] animated:YES];
   }
}

- (IBAction)stopBluetooth
{
   if ([WiiMoteHelper haveBluetooth])
   {
      [WiiMoteHelper stopBluetooth];
      [self.topViewController.navigationItem setRightBarButtonItem:[self createBluetoothButton] animated:YES];
   }
}

@end


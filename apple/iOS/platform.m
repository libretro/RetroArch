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

#include <pthread.h>
#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"

#include "../RetroArch/apple_input.h"

#import "views.h"
#include "input/BTStack/btpad.h"
#include "input/BTStack/btdynamic.h"
#include "input/BTStack/btpad.h"

#include "file.h"

//#define HAVE_DEBUG_FILELOG

// Input helpers: This is kept here because it needs objective-c
static void handle_touch_event(NSArray* touches)
{
   const int numTouches = [touches count];
   const float scale = [[UIScreen mainScreen] scale];

   g_current_input_data.touch_count = 0;
   
   for(int i = 0; i != numTouches && g_current_input_data.touch_count < MAX_TOUCHES; i ++)
   {
      UITouch* touch = [touches objectAtIndex:i];
      const CGPoint coord = [touch locationInView:touch.view];

      if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
      {
         g_current_input_data.touches[g_current_input_data.touch_count   ].screen_x = coord.x * scale;
         g_current_input_data.touches[g_current_input_data.touch_count ++].screen_y = coord.y * scale;
      }
   }
}

@interface RApplication : UIApplication
@end

@implementation RApplication

- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];
   
   if ([[event allTouches] count])
      handle_touch_event(event.allTouches.allObjects);
   else if ([event respondsToSelector:@selector(_gsEvent)])
   {
      // Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
      uint8_t* eventMem = (uint8_t*)(void*)CFBridgingRetain([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? *(int*)&eventMem[8] : 0;

      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
         apple_input_handle_key_event(*(uint16_t*)&eventMem[0x3C], eventType == GSEVENT_TYPE_KEYDOWN);

      CFBridgingRelease(eventMem);
   }
}

@end

@implementation RetroArch_iOS
{
   UIWindow* _window;

   bool _isGameTop, _isRomList;
   uint32_t _settingMenusInBackStack;
   uint32_t _enabledOrientations;
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

#pragma mark LIFECYCLE (UIApplicationDelegate)
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   apple_platform = self;
   self.delegate = self;

   // Setup window
   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = self;
   [_window makeKeyAndVisible];

   // Build system paths and test permissions
   self.documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
   self.systemDirectory = [self.documentsDirectory stringByAppendingPathComponent:@".RetroArch"];
   self.systemConfigPath = [self.systemDirectory stringByAppendingPathComponent:@"frontend.cfg"];

   if (!path_make_and_check_directory(self.documentsDirectory.UTF8String, 0755, R_OK | W_OK | X_OK))
      apple_display_alert([NSString stringWithFormat:@"Failed to create or access base directory: %@", self.documentsDirectory], 0);
   else if (!path_make_and_check_directory(self.systemDirectory.UTF8String, 0755, R_OK | W_OK | X_OK))
      apple_display_alert([NSString stringWithFormat:@"Failed to create or access system directory: %@", self.systemDirectory], 0);
   else
   {
      [self pushViewController:[RADirectoryList directoryListAtBrowseRoot] animated:YES];
      [self refreshSystemConfig];
      
      if (apple_use_tv_mode)
         apple_run_core(nil, 0);
   }
   
   // Warn if there are no cores present
   if ([RAModuleInfo getModules].count == 0)
      apple_display_alert(@"No libretro cores were found. You will not be able to play any games.", 0);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   apple_exit_stasis();
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   apple_enter_stasis();
}

// UINavigationControllerDelegate
- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   _isGameTop = [viewController isKindOfClass:[RAGameView class]];
   _isRomList = [viewController isKindOfClass:[RADirectoryList class]];

   [[UIApplication sharedApplication] setStatusBarHidden:_isGameTop withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:_isGameTop];

   self.navigationBarHidden = _isGameTop;
   [self setToolbarHidden:!_isRomList animated:YES];
   self.topViewController.navigationItem.rightBarButtonItem = [self createSettingsButton];
}

// UINavigationController: Never animate when pushing onto, or popping, an RAGameView
- (void)pushViewController:(UIViewController*)theView animated:(BOOL)animated
{
   if ([theView respondsToSelector:@selector(isSettingsView)] && [(id)theView isSettingsView])
      _settingMenusInBackStack ++;

   [super pushViewController:theView animated:animated && !_isGameTop];
}

- (UIViewController*)popViewControllerAnimated:(BOOL)animated
{
   if ([self.topViewController respondsToSelector:@selector(isSettingsView)] && [(id)self.topViewController isSettingsView])
      _settingMenusInBackStack --;

   return [super popViewControllerAnimated:animated && !_isGameTop];
}

// NOTE: This version only runs on iOS6
- (NSUInteger)supportedInterfaceOrientations
{
   return _isGameTop ? _enabledOrientations
                     : UIInterfaceOrientationMaskAll;
}

// NOTE: This version runs on iOS2-iOS5, but not iOS6
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
   if (_isGameTop)
      switch (interfaceOrientation)
      {
         case UIInterfaceOrientationPortrait:
            return (_enabledOrientations & UIInterfaceOrientationMaskPortrait);
         case UIInterfaceOrientationPortraitUpsideDown:
            return (_enabledOrientations & UIInterfaceOrientationMaskPortraitUpsideDown);
         case UIInterfaceOrientationLandscapeLeft:
            return (_enabledOrientations & UIInterfaceOrientationMaskLandscapeLeft);
         case UIInterfaceOrientationLandscapeRight:
            return (_enabledOrientations & UIInterfaceOrientationMaskLandscapeRight);
      }
   
   return YES;
}


#pragma mark RetroArch_Platform
- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file
{
   [self pushViewController:RAGameView.get animated:NO];
   [RASettingsList refreshModuleConfig:core];

   btpad_set_inquiry_state(false);

   [self refreshSystemConfig];
}

- (void)unloadingCore:(RAModuleInfo*)core
{
   [self popToViewController:[RAGameView get] animated:NO];
   [self popViewControllerAnimated:NO];
      
   btpad_set_inquiry_state(true);
}

- (NSString*)retroarchConfigPath
{
   return self.systemDirectory;
}

- (NSString*)corePath
{
   return [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"modules"];
}

#pragma mark FRONTEND CONFIG
- (void)refreshSystemConfig
{
   // Read load time settings
   config_file_t* conf = config_file_new([self.systemConfigPath UTF8String]);

   if (conf)
   {
      // Get enabled orientations
      static const struct { const char* setting; uint32_t orientation; } orientationSettings[4] =
      {
         { "ios_allow_portrait", UIInterfaceOrientationMaskPortrait },
         { "ios_allow_portrait_upside_down", UIInterfaceOrientationMaskPortraitUpsideDown },
         { "ios_allow_landscape_left", UIInterfaceOrientationMaskLandscapeLeft },
         { "ios_allow_landscape_right", UIInterfaceOrientationMaskLandscapeRight }
      };
   
      _enabledOrientations = 0;
   
      for (int i = 0; i < 4; i ++)
      {
         bool enabled = false;
         bool found = config_get_bool(conf, orientationSettings[i].setting, &enabled);
         
         if (!found || enabled)
            _enabledOrientations |= orientationSettings[i].orientation;
      }
      
      // Setup bluetooth mode
      NSString* btmode = objc_get_value_from_config(conf, @"ios_btmode", @"keyboard");
      apple_input_enable_icade([btmode isEqualToString:@"icade"]);
      btstack_set_poweron([btmode isEqualToString:@"btstack"]);

      bool val;
      apple_use_tv_mode = config_get_bool(conf, "ios_tv_mode", & val) && val;
      
      config_file_free(conf);
   }
}

#pragma mark PAUSE MENU
- (UIBarButtonItem*)createSettingsButton
{
   if (_settingMenusInBackStack == 0)
      return [[UIBarButtonItem alloc]
            initWithTitle:@"Settings"
                    style:UIBarButtonItemStyleBordered
                   target:[RetroArch_iOS get]
                   action:@selector(showSystemSettings)];
   
   else
      return nil;
}

- (IBAction)showPauseMenu:(id)sender
{
   if (apple_is_running && !apple_is_paused && _isGameTop)
   {
      apple_is_paused = true;
      [[RAGameView get] openPauseMenu];
      
      btpad_set_inquiry_state(true);
   }
}

- (IBAction)basicEvent:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(&apple_event_basic_command, ((UIView*)sender).tag);
   
   [self closePauseMenu:sender];
}

- (IBAction)chooseState:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(apple_event_set_state_slot, (void*)((UISegmentedControl*)sender).selectedSegmentIndex);
}

- (IBAction)showRGUI:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(apple_event_show_rgui, 0);
   
   [self closePauseMenu:sender];
}

- (IBAction)closePauseMenu:(id)sender
{
   [[RAGameView get] closePauseMenu];
   apple_is_paused = false;
   
   btpad_set_inquiry_state(false);
}

- (IBAction)showSettings
{
   [self pushViewController:[[RASettingsList alloc] initWithModule:apple_core] animated:YES];
}

- (IBAction)showSystemSettings
{
   [self pushViewController:[RASystemSettingsList new] animated:YES];
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
#if defined(HAVE_DEBUG_FILELOG) && (TARGET_IPHONE_SIMULATOR == 0)
      NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
      NSString *documentsDirectory = [paths objectAtIndex:0];
      NSString *logPath = [documentsDirectory stringByAppendingPathComponent:@"console_stdout.log"];
      freopen([logPath cStringUsingEncoding:NSASCIIStringEncoding], "a", stdout);
      freopen([logPath cStringUsingEncoding:NSASCIIStringEncoding], "a", stderr);
#endif
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

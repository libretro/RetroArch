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

#include "apple/common/apple_input.h"
#include "apple/common/setting_data.h"
#include "menu.h"

#import "views.h"
#include "bluetooth/btpad.h"
#include "bluetooth/btdynamic.h"
#include "bluetooth/btpad.h"

#include "file.h"

apple_frontend_settings_t apple_frontend_settings;

//#define HAVE_DEBUG_FILELOG
bool is_ios_7()
{
   return [[UIDevice currentDevice].systemVersion compare:@"7.0" options:NSNumericSearch] != NSOrderedAscending;
}

void ios_set_bluetooth_mode(NSString* mode)
{
   if (!is_ios_7())
   {
      apple_input_enable_icade([mode isEqualToString:@"icade"]);
      btstack_set_poweron([mode isEqualToString:@"btstack"]);
   }
#ifdef __IPHONE_7_0 // iOS7 iCade Support
   else
   {
      bool enabled = [mode isEqualToString:@"icade"];
      apple_input_enable_icade(enabled);
      [[RAGameView get] iOS7SetiCadeMode:enabled];
   }
#endif
}

const void* apple_get_frontend_settings(void)
{
   static rarch_setting_t settings[9];
   
   if (settings[0].type == ST_NONE)
   {
      settings[0] = setting_data_group_setting(ST_GROUP, "Frontend Settings");
      settings[1] = setting_data_group_setting(ST_SUB_GROUP, "Frontend");
      settings[2] = setting_data_bool_setting("ios_use_file_log", "Enable File Logging",
                                               &apple_frontend_settings.logging_enabled, false);
      settings[3] = setting_data_bool_setting("ios_tv_mode", "TV Mode", &apple_use_tv_mode, false);
      settings[4] = setting_data_string_setting(ST_STRING, "ios_btmode", "Bluetooth Input Type", apple_frontend_settings.bluetooth_mode,
                                                 sizeof(apple_frontend_settings.bluetooth_mode), "none");                                                 

      // Set ios_btmode options based on runtime environment
      if (is_ios_7())
         settings[4].values = "none|icade";
      else if (btstack_try_load())
         settings[4].values = "none|icade|keyboard|btstack";
      else
         settings[4].values = "none|icade|keyboard";

      settings[5] = setting_data_string_setting(ST_STRING, "ios_orientations", "Screen Orientations", apple_frontend_settings.orientations,
                                                 sizeof(apple_frontend_settings.orientations), "both");
      settings[5].values = "both|landscape|portrait";
      settings[6] = setting_data_group_setting(ST_END_SUB_GROUP, 0);
      
      settings[7] = setting_data_group_setting(ST_END_GROUP, 0);
   }
   
   return settings;
}

void ios_set_logging_state(const char *log_path, bool on)
{
   fflush(stdout);
   fflush(stderr);
   
   if (on && !apple_frontend_settings.logging.file)
   {
      apple_frontend_settings.logging.file = fopen(log_path, "a");
      apple_frontend_settings.logging.stdout = dup(1);
      apple_frontend_settings.logging.stderr = dup(2);
      dup2(fileno(apple_frontend_settings.logging.file), 1);
      dup2(fileno(apple_frontend_settings.logging.file), 2);
   }
   else if (!on && apple_frontend_settings.logging.file)
   {
      dup2(apple_frontend_settings.logging.stdout, 1);
      dup2(apple_frontend_settings.logging.stderr, 2);
      
      fclose(apple_frontend_settings.logging.file);
      apple_frontend_settings.logging.file = 0;
   }
}


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

#ifdef __IPHONE_7_0 // iOS7 iCade Support

- (NSArray*)keyCommands
{
   static NSMutableArray* key_commands;

   if (!key_commands)
   {
      key_commands = [NSMutableArray array];
   
      for (int i = 0; i < 26; i ++)
      {
         [key_commands addObject:[UIKeyCommand keyCommandWithInput:[NSString stringWithFormat:@"%c", 'a' + i]
                                               modifierFlags:0 action:@selector(keyGotten:)]];
      }
   }

   return key_commands;
}

- (void)keyGotten:(UIKeyCommand *)keyCommand
{
   apple_input_handle_key_event([keyCommand.input characterAtIndex:0] - 'a' + 4, true);
}

#endif

@end

@implementation RetroArch_iOS
{
   UIWindow* _window;
   NSString* _path;

   bool _isGameTop;
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
   
   self.configDirectory = self.systemDirectory;
   self.globalConfigFile = [NSString stringWithFormat:@"%@/retroarch.cfg", self.configDirectory];
   self.coreDirectory = [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"modules"];
   self.logPath = [self.systemDirectory stringByAppendingPathComponent:@"stdout.log"];
    
    const char *path = self.documentsDirectory.UTF8String;
    path_mkdir(path);
    if (access(path, 0755) != 0)
      apple_display_alert([NSString stringWithFormat:@"Failed to create or access base directory: %@", self.documentsDirectory], 0);
    else
    {
        path = self.systemDirectory.UTF8String;
        path_mkdir(path);
        if (access(path, 0755) != 0)
            apple_display_alert([NSString stringWithFormat:@"Failed to create or access system directory: %@", self.systemDirectory], 0);
        else
           [self pushViewController:[RAMainMenu new] animated:YES];
    }
   
   // Warn if there are no cores present
   apple_core_info_set_core_path(self.coreDirectory.UTF8String);
   apple_core_info_set_config_path(self.configDirectory.UTF8String);
   const core_info_list_t* core_list = apple_core_info_list_get();
   
   if (!core_list || core_list->count == 0)
      apple_display_alert(@"No libretro cores were found. You will not be able to run any content.", 0);
      
   // Load system config
   const rarch_setting_t* frontend_settings = apple_get_frontend_settings();   
   setting_data_reset(frontend_settings);
   setting_data_load_config_path(frontend_settings, self.systemConfigPath.UTF8String);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   apple_exit_stasis(false);
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   apple_enter_stasis();
}

#pragma mark Frontend Browsing Logic
-(BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
   NSString* filename = url.path.lastPathComponent;

   NSError* error = nil;
   [NSFileManager.defaultManager moveItemAtPath:url.path toPath:[self.documentsDirectory stringByAppendingPathComponent:filename] error:&error];
   
   if (error)
      printf("%s\n", error.description.UTF8String);
   
   return true;
}

// UINavigationControllerDelegate
- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   apple_input_reset_icade_buttons();
   _isGameTop = [viewController isKindOfClass:[RAGameView class]];
   g_extern.is_paused = !_isGameTop;

   [[UIApplication sharedApplication] setStatusBarHidden:_isGameTop withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:_isGameTop];

   [self setNavigationBarHidden:_isGameTop animated:!_isGameTop];
   [self setToolbarHidden:!viewController.toolbarItems.count animated:YES];
   
   // Workaround to keep frontend settings fresh
   [self refreshSystemConfig];
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
- (void)loadingCore:(NSString*)core withFile:(const char*)file
{
   [self pushViewController:RAGameView.get animated:NO];
   (void)[[RACoreSettingsMenu alloc] initWithCore:core];

   btpad_set_inquiry_state(false);

   [self refreshSystemConfig];
}

- (void)unloadingCore:(NSString*)core
{
   [self popToViewController:[RAGameView get] animated:NO];
   [self popViewControllerAnimated:NO];
      
   btpad_set_inquiry_state(true);
}

#pragma mark FRONTEND CONFIG
- (void)refreshSystemConfig
{
   // Get enabled orientations
   _enabledOrientations = UIInterfaceOrientationMaskAll;
   
   if (strcmp(apple_frontend_settings.orientations, "landscape") == 0)
      _enabledOrientations = UIInterfaceOrientationMaskLandscape;
   else if (strcmp(apple_frontend_settings.orientations, "portrait") == 0)
      _enabledOrientations = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;

   // Set bluetooth mode
   ios_set_bluetooth_mode(@(apple_frontend_settings.bluetooth_mode));
   ios_set_logging_state([RetroArch_iOS get].logPath.UTF8String, apple_frontend_settings.logging_enabled);
}

- (IBAction)showPauseMenu:(id)sender
{
   [self pushViewController:[RAPauseMenu new] animated:YES];
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

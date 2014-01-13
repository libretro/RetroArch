/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"

#include "apple/common/apple_input.h"
#include "apple/common/setting_data.h"
#include "apple/common/apple_gamecontroller.h"
#include "menu.h"

#import "views.h"
#include "bluetooth/btpad.h"
#include "bluetooth/btdynamic.h"
#include "bluetooth/btpad.h"

#include "file.h"

apple_frontend_settings_t apple_frontend_settings;

int get_ios_version_major(void)
{
   static int version = -1;
   
   if (version < 0)
      version = (int)[[[UIDevice currentDevice] systemVersion] floatValue];
   
   return version;
}

void ios_set_bluetooth_mode(NSString* mode)
{
   apple_input_enable_icade([mode isEqualToString:@"icade"]);
   btstack_set_poweron([mode isEqualToString:@"btstack"]);
}

static void apple_refresh_frontend_config(const rarch_setting_t* setting)
{
   [[RetroArch_iOS get] refreshSystemConfig];
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
      settings[2].change_handler = apple_refresh_frontend_config;
      settings[3] = setting_data_bool_setting("ios_tv_mode", "TV Mode", &apple_use_tv_mode, false);
      settings[4] = setting_data_string_setting(ST_STRING, "ios_btmode", "Bluetooth Input Type", apple_frontend_settings.bluetooth_mode,
                                                 sizeof(apple_frontend_settings.bluetooth_mode), "none");                                                 
      settings[4].change_handler = apple_refresh_frontend_config;

      // Set ios_btmode options based on runtime environment
      if (btstack_try_load())
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
      const CGPoint coord = [touch locationInView:[touch view]];

      if ([touch phase] != UITouchPhaseEnded && [touch phase] != UITouchPhaseCancelled)
      {
         g_current_input_data.touches[g_current_input_data.touch_count   ].screen_x = coord.x * scale;
         g_current_input_data.touches[g_current_input_data.touch_count ++].screen_y = coord.y * scale;
      }
   }
}

// iO7 Keyboard support
@interface UIEvent(iOS7Keyboard)
@property(readonly, nonatomic) long long _keyCode;
@property(readonly, nonatomic) _Bool _isKeyDown;
@property(retain, nonatomic) NSString *_privateInput;
@property(nonatomic) long long _modifierFlags;
- (struct __IOHIDEvent { }*)_hidEvent;
@end

@interface UIApplication(iOS7Keyboard)
- (id)_keyCommandForEvent:(id)event;
@end

@interface RApplication : UIApplication
@end

@implementation RApplication

// Keyboard handler for iOS 7
- (id)_keyCommandForEvent:(UIEvent*)event
{
   // This gets called twice with the same timestamp for each keypress, that's fine for polling
   // but is bad for business with events.
   static double last_time_stamp;
   
   if (last_time_stamp == [event timestamp])
      return [super _keyCommandForEvent:event];
   last_time_stamp = [event timestamp];
   
   // If the _hidEvent is null, [event _keyCode] will crash. (This happens with the on screen keyboard.)
   if ([event _hidEvent])
   {
      NSString* ch = [event _privateInput];
      
      if (!ch || [ch length] == 0)
         apple_input_keyboard_event([event _isKeyDown], [event _keyCode], 0, [event _modifierFlags]);
      else
      {
         apple_input_keyboard_event([event _isKeyDown], [event _keyCode], [ch characterAtIndex:0], [event _modifierFlags]);
         
         for (unsigned i = 1; i != [ch length]; i ++)
            apple_input_keyboard_event([event _isKeyDown], 0, [ch characterAtIndex:i], [event _modifierFlags]);
      }
   }

   return [super _keyCommandForEvent:event];
}

- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];
   
   if ([[event allTouches] count])
      handle_touch_event([[event allTouches] allObjects]);

   if (!(IOS_IS_VERSION_7_OR_HIGHER()) && [event respondsToSelector:@selector(_gsEvent)])
   {
      // Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
      const uint8_t* eventMem = objc_unretainedPointer([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? *(int*)&eventMem[8] : 0;
      
      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
         apple_input_keyboard_event(eventType == GSEVENT_TYPE_KEYDOWN, *(uint16_t*)&eventMem[0x3C], 0, 0);
   }
}

@end

@implementation RetroArch_iOS
{
   UIWindow* _window;
   NSString* _path;
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   apple_platform = self;
   [self setDelegate:self];

   // Setup window
   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self showPauseMenu:self];
   [_window makeKeyAndVisible];

   // Build system paths and test permissions
   self.documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
   self.systemDirectory = [self.documentsDirectory stringByAppendingPathComponent:@".RetroArch"];
   self.systemConfigPath = [self.systemDirectory stringByAppendingPathComponent:@"frontend.cfg"];
   
   self.configDirectory = self.systemDirectory;
   self.globalConfigFile = [NSString stringWithFormat:@"%@/retroarch.cfg", self.configDirectory];
   self.coreDirectory = [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"modules"];
   self.logPath = [self.systemDirectory stringByAppendingPathComponent:@"stdout.log"];
    
    const char *path = [self.documentsDirectory UTF8String];
    path_mkdir(path);
    if (access(path, 0755) != 0)
      apple_display_alert([NSString stringWithFormat:@"Failed to create or access base directory: %@", self.documentsDirectory], 0);
    else
    {
        path = [self.systemDirectory UTF8String];
        path_mkdir(path);
        if (access(path, 0755) != 0)
            apple_display_alert([NSString stringWithFormat:@"Failed to create or access system directory: %@", self.systemDirectory], 0);
        else
           [self pushViewController:[RAMainMenu new] animated:YES];
    }
   
   // Warn if there are no cores present
   apple_core_info_set_core_path([self.coreDirectory UTF8String]);
   apple_core_info_set_config_path([self.configDirectory UTF8String]);
   const core_info_list_t* core_list = apple_core_info_list_get();
   
   if (!core_list || core_list->count == 0)
      apple_display_alert(@"No libretro cores were found. You will not be able to run any content.", 0);
   
   apple_gamecontroller_init();
   
   // Load system config
   const rarch_setting_t* frontend_settings = apple_get_frontend_settings();   
   setting_data_reset(frontend_settings);
   setting_data_load_config_path(frontend_settings, [self.systemConfigPath UTF8String]);
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   apple_start_iteration();
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   apple_stop_iteration();
}

-(BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
   NSString* filename = [[url path] lastPathComponent];

   NSError* error = nil;
   [[NSFileManager defaultManager] moveItemAtPath:[url path] toPath:[self.documentsDirectory stringByAppendingPathComponent:filename] error:&error];
   
   if (error)
      printf("%s\n", [[error description] UTF8String]);
   
   return true;
}

// UINavigationControllerDelegate
- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   apple_input_reset_icade_buttons();
   [self setToolbarHidden:![[viewController toolbarItems] count] animated:YES];
   
   // Workaround to keep frontend settings fresh
   [self refreshSystemConfig];
}

- (void)showGameView
{
   [self popToRootViewControllerAnimated:NO];
   [self setToolbarHidden:true animated:NO];
   [[UIApplication sharedApplication] setStatusBarHidden:true withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:true];
   [_window setRootViewController:[RAGameView get]];
   g_extern.is_paused = false;
}

- (IBAction)showPauseMenu:(id)sender
{
   g_extern.is_paused = true;
   [[UIApplication sharedApplication] setStatusBarHidden:false withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:false];
   [_window setRootViewController:self];
}

- (void)loadingCore:(NSString*)core withFile:(const char*)file
{
   (void)[[RACoreSettingsMenu alloc] initWithCore:core];

   btpad_set_inquiry_state(false);

   [self refreshSystemConfig];
   [self showGameView];
}

- (void)unloadingCore:(NSString*)core
{
   [self showPauseMenu:self];
   
   btpad_set_inquiry_state(true);
}

- (void)refreshSystemConfig
{
   // Get enabled orientations
   apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskAll;
   
   if (strcmp(apple_frontend_settings.orientations, "landscape") == 0)
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskLandscape;
   else if (strcmp(apple_frontend_settings.orientations, "portrait") == 0)
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;

   // Set bluetooth mode
   ios_set_bluetooth_mode(BOXSTRING(apple_frontend_settings.bluetooth_mode));
   ios_set_logging_state([[RetroArch_iOS get].logPath UTF8String], apple_frontend_settings.logging_enabled);
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

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

#include "apple_input.h"

#ifdef IOS
#import "views.h"
#include "../iOS/input/BTStack/btpad.h"
#include "../iOS/input/BTStack/btdynamic.h"
#include "../iOS/input/BTStack/btpad.h"
#endif

#include "file.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

//#define HAVE_DEBUG_FILELOG
static bool use_tv_mode;

id<RetroArch_Platform> apple_platform;

// From frontend/frontend_ios.c
extern void* rarch_main_apple(void* args);
extern void apple_frontend_post_event(void (*fn)(void*), void* userdata);


// These are based on the tag property of the button used to trigger the event
enum basic_event_t { RESET = 1, LOAD_STATE = 2, SAVE_STATE = 3, QUIT = 4 };
static void event_basic_command(void* userdata)
{
   switch ((enum basic_event_t)userdata)
   {
      case RESET:      rarch_game_reset(); return;
      case LOAD_STATE: rarch_load_state(); return;
      case SAVE_STATE: rarch_save_state(); return;
      case QUIT:       g_extern.system.shutdown = true; return;
   }
}

static void event_set_state_slot(void* userdata)
{
   g_extern.state_slot = (uint32_t)userdata;
}

static void event_show_rgui(void* userdata)
{
   const bool in_menu = g_extern.lifecycle_mode_state & (1 << MODE_MENU);
   g_extern.lifecycle_mode_state &= ~(1ULL << (in_menu ? MODE_MENU : MODE_GAME));
   g_extern.lifecycle_mode_state |=  (1ULL << (in_menu ? MODE_GAME : MODE_MENU));
}

static void event_reload_config(void* userdata)
{
   objc_clear_config_hack();

   uninit_drivers();
   config_load();
   init_drivers();
}

#pragma mark EMULATION
static pthread_t apple_retro_thread;
static bool apple_is_paused;
static bool apple_is_running;
static RAModuleInfo* apple_core;

// HACK: This needs to be cleaned
void apple_run_core(RAModuleInfo* core, const char* file)
{
   if (!apple_is_running)
   {
      [apple_platform loadingCore:core withFile:file];

      apple_core = core;
      apple_is_running = true;
      
      struct rarch_main_wrap* load_data = malloc(sizeof(struct rarch_main_wrap));
      memset(load_data, 0, sizeof(struct rarch_main_wrap));

      load_data->config_path = strdup(apple_platform.retroarchConfigPath.UTF8String);

#ifdef IOS
      load_data->sram_path = strdup(RetroArch_iOS.get.systemDirectory.UTF8String);
      load_data->state_path = strdup(RetroArch_iOS.get.systemDirectory.UTF8String);
#endif

      if (file && core)
      {
         load_data->libretro_path = strdup(apple_core.path.UTF8String);
         load_data->rom_path = strdup(file);
      }
      
      if (pthread_create(&apple_retro_thread, 0, rarch_main_apple, load_data))
      {
         apple_rarch_exited((void*)1);
         return;
      }
      
      pthread_detach(apple_retro_thread);
   }
}

void apple_rarch_exited(void* result)
{
   if (result)
      apple_display_alert(@"Failed to load game.", 0);

   RAModuleInfo* used_core = apple_core;
   apple_core = nil;

   if (apple_is_running)
   {
      apple_is_running = false;
      [apple_platform unloadingCore:used_core];
   }

   if (use_tv_mode)
      apple_run_core(nil, 0);
}

//
// IOS
//
#pragma mark IOS
#ifdef IOS
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

@end

@implementation RetroArch_iOS
{
   UIWindow* _window;

   bool _isGameTop;
   uint32_t _settingMenusInBackStack;
   uint32_t _enabledOrientations;
   
   RAModuleInfo* _module;
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

// UIApplicationDelegate
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
      
      if (use_tv_mode)
         apple_run_core(nil, 0);
   }
   
   // Warn if there are no cores present
   if ([RAModuleInfo getModules].count == 0)
      apple_display_alert(@"No libretro cores were found. You will not be able to play any games.", 0);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
   [RAGameView.get resume];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   [RAGameView.get suspend];
}

// UINavigationControllerDelegate
- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   _isGameTop = [viewController isKindOfClass:[RAGameView class]];

   [[UIApplication sharedApplication] setStatusBarHidden:_isGameTop withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:_isGameTop];

   self.navigationBarHidden = _isGameTop;
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
   return [NSString stringWithFormat:@"%@/retroarch.cfg", self.systemDirectory];
}

- (NSString*)corePath
{
   return [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"modules"];
}

- (void)refreshConfig
{
   if (apple_is_running)
      apple_frontend_post_event(&event_reload_config, 0);
   else
      objc_clear_config_hack();
}

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
      
      //
      bool val;
      apple_input_enable_icade(config_get_bool(conf, "ios_use_icade", &val) && val);
      btstack_set_poweron(config_get_bool(conf, "ios_use_btstack", &val) && val);
      use_tv_mode = config_get_bool(conf, "ios_tv_mode", & val) && val;
      
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
      apple_frontend_post_event(&event_basic_command, ((UIView*)sender).tag);
   
   [self closePauseMenu:sender];
}

- (IBAction)chooseState:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(event_set_state_slot, (void*)((UISegmentedControl*)sender).selectedSegmentIndex);
}

- (IBAction)showRGUI:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(event_show_rgui, 0);
   
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
   [self pushViewController:[[RASettingsList alloc] initWithModule:_module] animated:YES];
}

- (IBAction)showSystemSettings
{
   [self pushViewController:[RASystemSettingsList new] animated:YES];
}

@end


#endif

//
// OSX
//
#pragma mark OSX
#ifdef OSX

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   [super sendEvent:event];

   if (event.type == GSEVENT_TYPE_KEYDOWN || event.type == GSEVENT_TYPE_KEYUP)
      apple_input_handle_key_event(event.keyCode, event.type == GSEVENT_TYPE_KEYDOWN);
}

@end

@implementation RetroArch_OSX
{
   NSWindow IBOutlet* _coreSelectSheet;

   bool _wantReload;
   NSString* _file;
   RAModuleInfo* _core;
}

+ (RetroArch_OSX*)get
{
   return (RetroArch_OSX*)[[NSApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   apple_platform = self;

   [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

   window.backgroundColor = [NSColor blackColor];
   [window.contentView setAutoresizesSubviews:YES];
   
   RAGameView.get.frame = [window.contentView bounds];
   [window.contentView addSubview:RAGameView.get];
   
   [window makeFirstResponder:RAGameView.get];
   
   // Create core select list
   NSComboBox* cb = (NSComboBox*)[_coreSelectSheet.contentView viewWithTag:1];
   
   for (RAModuleInfo* i in RAModuleInfo.getModules)
      [cb addItemWithObjectValue:i];
   
   // Run RGUI if needed
   if (!apple_is_running)
      apple_run_core(nil, 0);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
   return YES;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
   if (filename)
   {
      _file = filename;
      [self chooseCore];
   }
   return YES;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if (filenames.count == 1 && filenames[0])
   {
      _file = filenames[0];
      [self chooseCore];
   }
   else
      apple_display_alert(@"Cannot open multiple files", @"RetroArch");
}

- (void)openDocument:(id)sender
{
   NSOpenPanel* panel = [NSOpenPanel openPanel];
   [panel beginSheetModalForWindow:window completionHandler:^(NSInteger result)
   {
      [NSApplication.sharedApplication stopModal];
   
      if (result == NSOKButton && panel.URL)
      {
         _file = panel.URL.path;
         [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
      }
   }];
   [NSApplication.sharedApplication runModalForWindow:panel];
}

- (void)chooseCore
{
   [NSApplication.sharedApplication beginSheet:_coreSelectSheet modalForWindow:window modalDelegate:nil didEndSelector:nil contextInfo:nil];
   [NSApplication.sharedApplication runModalForWindow:_coreSelectSheet];
}

- (IBAction)coreWasChosen:(id)sender
{
   [NSApplication.sharedApplication stopModal];
   [NSApplication.sharedApplication endSheet:_coreSelectSheet returnCode:0];
   [_coreSelectSheet orderOut:self];

   NSComboBox* cb = (NSComboBox*)[_coreSelectSheet.contentView viewWithTag:1];
   _core = (RAModuleInfo*)cb.objectValueOfSelectedItem;

   if (!apple_is_running)
      apple_run_core(_core, _file.UTF8String);
   else
   {
      _wantReload = true;
      apple_frontend_post_event(event_basic_command, (void*)QUIT);
   }
}

#pragma mark RetroArch_Platform
- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file
{
   if (file)
      [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:file]]];
}

- (void)unloadingCore:(RAModuleInfo*)core
{
   if (_wantReload)
      apple_run_core(_core, _file.UTF8String);
   
   _wantReload = false;
}

- (NSString*)retroarchConfigPath
{
   NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
   return [paths[0] stringByAppendingPathComponent:@"RetroArch/retroarch.cfg"];
}

- (NSString*)corePath
{
   return [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Contents/Resources/modules"];
}

#pragma mark Menus
- (IBAction)basicEvent:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(&event_basic_command, (void*)((NSMenuItem*)sender).tag);
}

@end

int main(int argc, char *argv[])
{
   return NSApplicationMain(argc, (const char **) argv);
}

#endif

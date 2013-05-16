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

#import <UIKit/UIKit.h>
#include "input/ios_input.h"
#include "input/keycode.h"
#include "input/BTStack/btpad.h"
#include "libretro.h"

#include <sys/stat.h>
#include <pthread.h>

#include "rarch_wrapper.h"
#include "general.h"
#include "frontend/menu/rmenu.h"

#import "browser/browser.h"
#import "settings/settings.h"

#include "input/BTStack/btdynamic.h"
#include "input/BTStack/btpad.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11
#define GSEVENT_TYPE_MODS 12
#define GSEVENT_MOD_CMD (1 << 16)
#define GSEVENT_MOD_SHIFT (1 << 17)
#define GSEVENT_MOD_ALT (1 << 19)
#define GSEVENT_MOD_CTRL (1 << 20)

//#define HAVE_DEBUG_FILELOG

static ios_input_data_t g_input_data;

static bool enable_btstack;
static bool use_icade;
static uint32_t icade_buttons;

// Input helpers
void ios_copy_input(ios_input_data_t* data)
{
   // Call only from main thread

   memcpy(data, &g_input_data, sizeof(g_input_data));
   data->pad_buttons = btpad_get_buttons() | (use_icade ? icade_buttons : 0);
   
   for (int i = 0; i < 4; i ++)
      data->pad_axis[i] = btpad_get_axis(i);
}

static void handle_touch_event(NSArray* touches)
{
   const int numTouches = [touches count];
   const float scale = [[UIScreen mainScreen] scale];

   g_input_data.touch_count = 0;
   
   for(int i = 0; i != numTouches && g_input_data.touch_count < MAX_TOUCHES; i ++)
   {
      UITouch* touch = [touches objectAtIndex:i];
      const CGPoint coord = [touch locationInView:touch.view];

      if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
      {
         g_input_data.touches[g_input_data.touch_count   ].screen_x = coord.x * scale;
         g_input_data.touches[g_input_data.touch_count ++].screen_y = coord.y * scale;
      }
   }
}

static void handle_icade_event(unsigned keycode)
{
   static const struct
   {
      bool up;
      int button;
   }  icade_map[0x20] =
   {
      { false, -1 }, { false, -1 }, { false, -1 }, { false, -1 }, // 0
      { false,  2 }, { false, -1 }, { true ,  3 }, { false,  3 }, // 4
      { true ,  0 }, { true,   5 }, { true ,  7 }, { false,  8 }, // 8
      { false,  6 }, { false,  9 }, { false, 10 }, { false, 11 }, // C
      { true ,  6 }, { true ,  9 }, { false,  7 }, { true,  10 }, // 0
      { true ,  2 }, { true ,  8 }, { false, -1 }, { true ,  4 }, // 4
      { false,  5 }, { true , 11 }, { false,  0 }, { false,  1 }, // 8
      { false,  4 }, { true ,  1 }, { false, -1 }, { false, -1 }  // C
   };
      
   if ((keycode < 0x20) && (icade_map[keycode].button >= 0))
   {
      const int button = icade_map[keycode].button;
      
      if (icade_map[keycode].up)
         icade_buttons &= ~(1 << button);
      else
         icade_buttons |=  (1 << button);
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
      {
         uint16_t key = *(uint16_t*)&eventMem[0x3C];

         if (!use_icade && key < MAX_KEYS)
            g_input_data.keys[key] = (eventType == GSEVENT_TYPE_KEYDOWN);
         else if (eventType == GSEVENT_TYPE_KEYDOWN)
            handle_icade_event(key);
      }

      CFBridgingRelease(eventMem);
   }
}

@end

int main(int argc, char *argv[])
{
#ifdef HAVE_DEBUG_FILELOG
#if TARGET_IPHONE_SIMULATOR == 0
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentsDirectory = [paths objectAtIndex:0];
	NSString *logPath = [documentsDirectory stringByAppendingPathComponent:@"console_stdout.log"];
	freopen([logPath cStringUsingEncoding:NSASCIIStringEncoding], "a", stdout);
	freopen([logPath cStringUsingEncoding:NSASCIIStringEncoding], "a", stderr);
#endif
#endif
    @autoreleasepool {
        return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
    }
}

#define kDOCSFOLDER [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"]

// From frontend/frontend_ios.c
extern void* rarch_main_ios(void* args);
extern void ios_frontend_post_event(void (*fn)(void*), void* userdata);

static void event_game_reset(void* userdata)
{
   rarch_game_reset();
}

static void event_load_state(void* userdata)
{
   rarch_load_state();
}

static void event_save_state(void* userdata)
{
   rarch_save_state();
}

static void event_set_state_slot(void* userdata)
{
   g_extern.state_slot = (uint32_t)userdata;
}

static void event_quit(void* userdata)
{
   g_extern.system.shutdown = true;
}

static void event_reload_config(void* userdata)
{
   // Need to clear these otherwise stale versions may be used!
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));

   uninit_drivers();
   g_extern.block_config_read = false;
   config_load();
   init_drivers();
}

@implementation RetroArch_iOS
{
   UIWindow* _window;

   pthread_t _retroThread;

   bool _isGameTop;
   bool _isPaused;
   bool _isRunning;
   
   RAModuleInfo* _module;
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
   self.system_directory = [NSString stringWithFormat:@"%@/.RetroArch", kDOCSFOLDER];
   self.systemConfigPath = [NSString stringWithFormat:@"%@/.RetroArch/frontend.cfg", kDOCSFOLDER];
   mkdir([self.system_directory UTF8String], 0755);

   // Setup window
   self.delegate = self;
   [self pushViewController:[RADirectoryList directoryListOrGridWithPath:kDOCSFOLDER] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = self;
   [_window makeKeyAndVisible];

   [self refreshSystemConfig];
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
   self.navigationBarHidden = _isGameTop;

   self.topViewController.navigationItem.rightBarButtonItem = [self createSettingsButton];
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
- (void)runGame:(NSString*)path withModule:(RAModuleInfo*)module
{
   if (_isRunning)
      return;

   _module = module;
   
   [RASettingsList refreshModuleConfig:module];
   
   [self pushViewController:RAGameView.get animated:NO];
   _isRunning = true;

   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf = (ra_ios_is_file(_module.configPath)) ? [_module.configPath UTF8String] : 0;
   const char* const libretro = [_module.path UTF8String];

   struct rarch_main_wrap* load_data = malloc(sizeof(struct rarch_main_wrap));
   load_data->libretro_path = strdup(libretro);
   load_data->rom_path = strdup([path UTF8String]);
   load_data->sram_path = strdup(sd);
   load_data->state_path = strdup(sd);
   load_data->verbose = false;
   load_data->config_path = strdup(cf);
   if (pthread_create(&_retroThread, 0, rarch_main_ios, load_data))
   {
      [self rarchExited:NO];
      return;
   }
   pthread_detach(_retroThread);

   [self refreshSystemConfig];
}

- (void)rarchExited:(BOOL)successful
{
   if (!successful)
   {
      [RetroArch_iOS displayErrorMessage:@"Failed to load game."];
   }

   if (_isRunning)
   {
      _isRunning = false;
      
      //
      [self popToViewController:[RAGameView get] animated:NO];
      [self popViewControllerAnimated:NO];
   }
   
   _module = nil;
}

- (void)refreshConfig
{
   if (_isRunning)
      ios_frontend_post_event(&event_reload_config, 0);
   else
   {
      // Need to clear these otherwise stale versions may be used!
      memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
      memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));
   }
}

- (void)refreshSystemConfig
{
   // Read load time settings
   config_file_t* conf = config_file_new([self.systemConfigPath UTF8String]);

   if (conf)
   {
      config_get_bool(conf, "ios_use_icade", &use_icade);
      config_get_bool(conf, "ios_use_btstack", &enable_btstack);
      
      if (enable_btstack)
         [self startBluetooth];
      else
         [self stopBluetooth];
   }

   config_file_free(conf);
}

#pragma mark PAUSE MENU
- (UIBarButtonItem*)createSettingsButton
{
   return [[UIBarButtonItem alloc]
         initWithTitle:@"Settings"
                 style:UIBarButtonItemStyleBordered
                target:[RetroArch_iOS get]
                action:@selector(showSystemSettings)];
}

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
   if (_isRunning)
      ios_frontend_post_event(&event_game_reset, 0);
   
   [self closePauseMenu:sender];
}

- (IBAction)loadState:(id)sender
{
   if (_isRunning)
      ios_frontend_post_event(&event_load_state, 0);

   [self closePauseMenu:sender];
}

- (IBAction)saveState:(id)sender
{
   if (_isRunning)
      ios_frontend_post_event(&event_save_state, 0);

   [self closePauseMenu:sender];
}

- (IBAction)chooseState:(id)sender
{
   if (_isRunning)
      ios_frontend_post_event(event_set_state_slot, (void*)((UISegmentedControl*)sender).selectedSegmentIndex);
}

- (IBAction)closePauseMenu:(id)sender
{
   [[RAGameView get] closePauseMenu];
   _isPaused = false;
}

- (IBAction)closeGamePressed:(id)sender
{
   [self closePauseMenu:sender];
   ios_frontend_post_event(event_quit, 0);
}

- (IBAction)showSettings
{
   if (_module)
      [self pushViewController:[[RASettingsList alloc] initWithModule:_module] animated:YES];
}

- (IBAction)showSystemSettings
{
   [self pushViewController:[RASystemSettingsList new] animated:YES];
}

#pragma mark Bluetooth Helpers
- (IBAction)startBluetooth
{
   if (btstack_is_loaded() && !btstack_is_running())
      btstack_start();
}

- (IBAction)stopBluetooth
{
   if (btstack_is_loaded())
      btstack_stop();
}

@end

void ios_rarch_exited(void* result)
{
   [[RetroArch_iOS get] rarchExited:result ? NO : YES];
}

char* ios_get_rarch_system_directory()
{
   return strdup([RetroArch_iOS.get.system_directory UTF8String]);
}

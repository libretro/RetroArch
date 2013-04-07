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

static ios_input_data_t g_input_data;

// Input helpers
void ios_copy_input(ios_input_data_t* data)
{
   // Call only from main thread

   memcpy(data, &g_input_data, sizeof(g_input_data));
   data->pad_buttons = btpad_get_buttons();
   
   for (int i = 0; i < 4; i ++)
      data->pad_axis[i] = btpad_get_axis(i);
}

static uint32_t translate_mods(uint32_t flags)
{
   uint32_t result = 0;
   if (flags & GSEVENT_MOD_ALT)   result |= RETROKMOD_ALT;
   if (flags & GSEVENT_MOD_CMD)   result |= RETROKMOD_META;
   if (flags & GSEVENT_MOD_SHIFT) result |= RETROKMOD_SHIFT;
   if (flags & GSEVENT_MOD_CTRL)  result |= RETROKMOD_CTRL;
   return result;
}

@interface RApplication : UIApplication
@end

@implementation RApplication

- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];
   
   if ([[event allTouches] count])
   {
      NSArray* touches = [[event allTouches] allObjects];
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
   // Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
   // TODO: Key events need to be synced, I just disabled them because the data isn't available on device (only in simulator)
   else if ([event respondsToSelector:@selector(_gsEvent)])
   {
      uint8_t* eventMem = (uint8_t*)(void*)CFBridgingRetain([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? *(int*)&eventMem[8] : 0;
      
      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
      {
         uint16_t* data = (uint16_t*)&eventMem[0x3C];

         if (data[0] < MAX_KEYS)
            g_input_data.keys[data[0]] = (eventType == GSEVENT_TYPE_KEYDOWN) ? 1 : 0;

         // Key events
         // ios_add_key_event(eventType == GSEVENT_TYPE_KEYDOWN, data[0], data[1], translate_mods(*(uint32_t*)&eventMem[0x30]));
         // printf("%d %d %d %08X\n", data[0], data[1], data[2], *(uint32_t*)&eventMem[0x30]);
      }
#if 0
      else if(eventType == GSEVENT_TYPE_MODS)
      {
         static const struct
         {
            unsigned key;
            unsigned retrokey;
            uint32_t hidid;
         }  modmap[] =
         {
            { 0x37, RETROK_LMETA, KEY_LeftGUI },
            { 0x36, RETROK_RMETA, KEY_RightGUI },
            { 0x38, RETROK_LSHIFT, KEY_LeftShift },
            { 0x3C, RETROK_RSHIFT, KEY_RightShift },
            { 0x3A, RETROK_LALT, KEY_LeftAlt },
            { 0x3D, RETROK_RALT, KEY_RightAlt },
            { 0x3B, RETROK_LCTRL, KEY_LeftControl },
            { 0x3E, RETROK_RCTRL, KEY_RightControl },
            { 0x39, RETROK_CAPSLOCK, KEY_CapsLock },
            { 0, RETROK_UNKNOWN, 0}
         };
         
         static bool keystate[9];
         
         // TODO: Not sure how to add this.
         //       The key value indicates the key that was pressed or released.
         //       The flags indicates the current modifier state.
         //       There is no way to determine if this is a keydown or a keyup event,
         //       except to look at the flags, but the bits in flags are shared between
         //       the left and right versions of a given key pair.
         //       The current method assumes that all key up and down events are processed,
         //       otherwise it may become confused.
         const uint32_t key = *(uint32_t*)&eventMem[0x3C];
         
         for (int i = 0; i < 9; i ++)
         {
            if (key == modmap[i].key)
            {
               keystate[i] = !keystate[i];
               g_input_data.keys[modmap[i].hidid] = keystate[i];
               // ios_add_key_event(keystate[i], modmap[i].retrokey, 0, translate_mods(*(uint32_t*)&eventMem[0x30]));
            }
         }
      }
#endif

      CFBridgingRelease(eventMem);
   }
}

@end

int main(int argc, char *argv[])
{
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
   mkdir([self.system_directory UTF8String], 0755);
         
   // Setup window
   self.delegate = self;
   [self pushViewController:[RADirectoryList directoryListOrGridWithPath:kDOCSFOLDER] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = self;
   [_window makeKeyAndVisible];
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
   if (_isRunning)
      return;

   assert(self.moduleInfo);
   
   [RASettingsList refreshConfigFile];
   
   [self pushViewController:RAGameView.get animated:NO];
   _isRunning = true;

   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf = (ra_ios_is_file(self.moduleInfo.configPath)) ? [self.moduleInfo.configPath UTF8String] : 0;
   const char* const libretro = [self.moduleInfo.path UTF8String];

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
   
   // Read load time settings
   // TODO: Do this better
   config_file_t* conf = config_file_new([self.moduleInfo.configPath UTF8String]);
   bool autoStartBluetooth = false;
   if (conf && config_get_bool(conf, "ios_auto_bluetooth", &autoStartBluetooth) && autoStartBluetooth)
      [self startBluetooth];
   config_file_free(conf);
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
     
      // Stop bluetooth (might be annoying but forgetting could eat battery of device AND wiimote)
      [self stopBluetooth];
      
      //
      [self popToViewController:[RAGameView get] animated:NO];
      [self popViewControllerAnimated:NO];
   }
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
   [self pushViewController:[RASettingsList new] animated:YES];
}

#pragma mark Bluetooth Helpers
- (UIBarButtonItem*)createBluetoothButton
{
   if (btstack_is_loaded())
   {
      const bool isBTOn = btstack_is_running();
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
   if (btstack_is_loaded() && !btstack_is_running())
   {
      btstack_start();
     [self.topViewController.navigationItem setRightBarButtonItem:[self createBluetoothButton] animated:YES];
   }
}

- (IBAction)stopBluetooth
{
   if (btstack_is_loaded())
   {
      btstack_stop();
      [self.topViewController.navigationItem setRightBarButtonItem:[self createBluetoothButton] animated:YES];
   }
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

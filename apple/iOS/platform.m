/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../common/RetroArch_Apple.h"
#include "../../input/drivers/apple_input.h"
#include "../../settings.h"
#ifdef HAVE_MFI
#include "../common/apple_gamecontroller.h"
#endif
#include "menu.h"
#include "../../menu/menu.h"

#import "views.h"
#include "../../input/drivers_hid/btstack_hid.h"

#include "../../menu/drivers/ios.h"

id<RetroArch_Platform> apple_platform;
static CFRunLoopObserverRef iterate_observer;

void apple_rarch_exited(void);

void main_exit_save_config(void);

static void rarch_draw(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    runloop_t *runloop = rarch_main_get_ptr();
    int ret = 0;
    bool iterate = iterate_observer && !runloop->is_paused;
    
    if (!iterate)
        return;
    
    ret = rarch_main_iterate();
    
    if (ret == -1)
    {
        main_exit_save_config();
        main_exit(NULL);
        return;
    }
    
    if (runloop->is_idle)
        return;
    
    if (g_view)
        [g_view display];
    CFRunLoopWakeUp(CFRunLoopGetMain());
}

void apple_rarch_exited(void)
{
   [apple_platform unloadingCore];
}

apple_frontend_settings_t apple_frontend_settings;

int get_ios_version_major(void)
{
   static int version = -1;
   
   if (version < 0)
      version = (int)[[[UIDevice currentDevice] systemVersion] floatValue];
   
   return version;
}

const void* apple_get_frontend_settings(void)
{
   static rarch_setting_t settings[9];
   
   if (settings[0].type == ST_NONE)
   {
       const char *GROUP_NAME = "Frontend Settings";
       const char *SUBGROUP_NAME = "Frontend";
      settings[0] = setting_group_setting(ST_GROUP, "Frontend Settings");
      settings[1] = setting_group_setting(ST_SUB_GROUP, "Frontend");
      settings[2] = setting_string_setting(ST_STRING, "ios_btmode", "Bluetooth Input Type", apple_frontend_settings.bluetooth_mode,
                                                 sizeof(apple_frontend_settings.bluetooth_mode), "none", "<null>", GROUP_NAME, SUBGROUP_NAME, NULL, NULL);

      /* Set iOS_btmode options based on runtime environment. */
      if (btstack_try_load())
         settings[2].values = "icade|keyboard|small_keyboard|btstack";
      else
         settings[2].values = "icade|keyboard|small_keyboard";

      settings[3] = setting_string_setting(ST_STRING, "ios_orientations", "Screen Orientations", apple_frontend_settings.orientations,
                                                 sizeof(apple_frontend_settings.orientations), "both", "<null>", GROUP_NAME, SUBGROUP_NAME, NULL, NULL);
      settings[3].values = "both|landscape|portrait";
      settings[4] = setting_group_setting(ST_END_SUB_GROUP, 0);
      settings[5] = setting_group_setting(ST_END_GROUP, 0);
   }
   
   return settings;
}

extern float apple_gfx_ctx_get_native_scale(void);

/* Input helpers: This is kept here because it needs ObjC */
static void handle_touch_event(NSArray* touches)
{
   NSUInteger i;
   driver_t *driver          = driver_get_ptr();
   apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
   float scale               = apple_gfx_ctx_get_native_scale();

   if (!apple)
      return;

   apple->touch_count = 0;
   
   for (i = 0; i < touches.count && (apple->touch_count < MAX_TOUCHES); i++)
   {
      UITouch* touch = [touches objectAtIndex:i];
      
      if (touch.view != [RAGameView get].view)
         continue;

      const CGPoint coord = [touch locationInView:[touch view]];

      if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
      {
         apple->touches[apple->touch_count   ].screen_x = coord.x * scale;
         apple->touches[apple->touch_count ++].screen_y = coord.y * scale;
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
- (id)_keyCommandForEvent:(UIEvent*)event;
@end

@interface RApplication : UIApplication
@end

@implementation RApplication

/* Keyboard handler for iOS 7. */

/* This is copied here as it isn't
 * defined in any standard iOS header */
enum
{
   NSAlphaShiftKeyMask = 1 << 16,
   NSShiftKeyMask      = 1 << 17,
   NSControlKeyMask    = 1 << 18,
   NSAlternateKeyMask  = 1 << 19,
   NSCommandKeyMask    = 1 << 20,
   NSNumericPadKeyMask = 1 << 21,
   NSHelpKeyMask       = 1 << 22,
   NSFunctionKeyMask   = 1 << 23,
   NSDeviceIndependentModifierFlagsMask = 0xffff0000U
};

- (id)_keyCommandForEvent:(UIEvent*)event
{
   NSUInteger i;
   // This gets called twice with the same timestamp for each keypress, that's fine for polling
   // but is bad for business with events.
   static double last_time_stamp;
   
   if (last_time_stamp == event.timestamp)
      return [super _keyCommandForEvent:event];
   last_time_stamp = event.timestamp;
   
   // If the _hidEvent is null, [event _keyCode] will crash. (This happens with the on screen keyboard.)
   if (event._hidEvent)
   {
      NSString* ch = (NSString*)event._privateInput;
      uint32_t character = 0;
      uint32_t mod       = 0;
      
      mod |= (event._modifierFlags & NSAlphaShiftKeyMask) ? RETROKMOD_CAPSLOCK : 0;
      mod |= (event._modifierFlags & NSShiftKeyMask     ) ? RETROKMOD_SHIFT    : 0;
      mod |= (event._modifierFlags & NSControlKeyMask   ) ? RETROKMOD_CTRL     : 0;
      mod |= (event._modifierFlags & NSAlternateKeyMask ) ? RETROKMOD_ALT      : 0;
      mod |= (event._modifierFlags & NSCommandKeyMask   ) ? RETROKMOD_META     : 0;
      mod |= (event._modifierFlags & NSNumericPadKeyMask) ? RETROKMOD_NUMLOCK  : 0;
      
      if (ch && ch.length != 0)
      {
         character = [ch characterAtIndex:0];
         apple_input_keyboard_event(event._isKeyDown,
               (uint32_t)event._keyCode, 0, mod,
               RETRO_DEVICE_KEYBOARD);
         
         for (i = 1; i < ch.length; i++)
            apple_input_keyboard_event(event._isKeyDown,
                  0, [ch characterAtIndex:i], mod,
                  RETRO_DEVICE_KEYBOARD);
      }
      
      apple_input_keyboard_event(event._isKeyDown,
            (uint32_t)event._keyCode, character, mod,
            RETRO_DEVICE_KEYBOARD);
   }

   return [super _keyCommandForEvent:event];
}

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];
   
   if (event.allTouches.count)
      handle_touch_event(event.allTouches.allObjects);

   if (!(get_ios_version_major() >= 7) && [event respondsToSelector:@selector(_gsEvent)])
   {
      // Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
      const uint8_t* eventMem = objc_unretainedPointer([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? *(int*)&eventMem[8] : 0;
      
      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
         apple_input_keyboard_event(eventType == GSEVENT_TYPE_KEYDOWN,
               *(uint16_t*)&eventMem[0x3C], 0, 0, RETRO_DEVICE_KEYBOARD);
   }
}

@end

@implementation RetroArch_iOS

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

void switch_to_ios(void)
{
   RetroArch_iOS *ap;
   runloop_t *runloop = rarch_main_get_ptr();

   if (!apple_platform)
      return;
    
   ap = (RetroArch_iOS *)apple_platform;
   runloop->is_idle = true;
   [ap showPauseMenu:ap];
}

void notify_content_loaded(void)
{
   if (!apple_platform)
      return;
    
   RetroArch_iOS *ap = (RetroArch_iOS *)apple_platform;
   [ap showGameView];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   driver_t *driver = NULL;

   apple_platform = self;
   [self setDelegate:self];
    
   if (rarch_main(0, NULL))
       apple_rarch_exited();

   // Setup window
   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self.window makeKeyAndVisible];

   [self pushViewController:[RAMainMenu new] animated:YES];

   [apple_platform loadingCore:nil withFile:nil];

   if (rarch_main(0, NULL))
      apple_rarch_exited();

   driver = driver_get_ptr();
    
   if ( driver->menu_ctx && driver->menu_ctx == &menu_ctx_ios && driver->menu && driver->menu->userdata )
   {
     ios_handle_t *ih = (ios_handle_t*)driver->menu->userdata;
     ih->switch_to_ios = switch_to_ios;
     ih->notify_content_loaded = notify_content_loaded;
   }
   
#ifdef HAVE_MFI
   apple_gamecontroller_init();
#endif
    
   [self showPauseMenu:self];

   [self apple_start_iteration];
}

- (void) apple_start_iteration
{
    if (iterate_observer)
        return;
    
    iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting,
                                               true, 0, rarch_draw, 0);
    CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer, kCFRunLoopCommonModes);
}

- (void) apple_stop_iteration
{
    if (!iterate_observer)
        return;
    
    CFRunLoopObserverInvalidate(iterate_observer);
    CFRelease(iterate_observer);
    iterate_observer = NULL;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    [self apple_stop_iteration];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
  [self showGameView];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   dispatch_async(dispatch_get_main_queue(),
                  ^{
                      main_exit_save_config();
                  });
   [self showPauseMenu: self];
}

-(BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
   NSString* filename = (NSString*)url.path.lastPathComponent;

   NSError* error = nil;
   [[NSFileManager defaultManager] moveItemAtPath:[url path] toPath:[self.documentsDirectory stringByAppendingPathComponent:filename] error:&error];
   
   if (error)
      printf("%s\n", [[error description] UTF8String]);
   
   return true;
}

- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   apple_input_reset_icade_buttons();
   [self setToolbarHidden:![[viewController toolbarItems] count] animated:YES];
   
   // Workaround to keep frontend settings fresh
   [self refreshSystemConfig];
}

- (void)showGameView
{
   runloop_t *runloop = rarch_main_get_ptr();

   [self popToRootViewControllerAnimated:NO];
   [self setToolbarHidden:true animated:NO];
   [[UIApplication sharedApplication] setStatusBarHidden:true withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:true];
   [self.window setRootViewController:[RAGameView get]];

   runloop->is_paused = false;
   runloop->is_idle = false;
}

- (IBAction)showPauseMenu:(id)sender
{
   runloop_t *runloop = rarch_main_get_ptr();

   runloop->is_paused = true;
   runloop->is_idle   = true;
   [[UIApplication sharedApplication] setStatusBarHidden:false withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:false];
   [self.window setRootViewController:self];
}

- (void)loadingCore:(NSString*)core withFile:(const char*)file
{
   btpad_set_inquiry_state(false);

   [self refreshSystemConfig];
   [self showGameView];
}

- (void)unloadingCore
{
   [self showPauseMenu:self];
   btpad_set_inquiry_state(true);
}

- (void)refreshSystemConfig
{
   bool small_keyboard, is_icade, is_btstack;
    
   /* Get enabled orientations */
   apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskAll;
   
   if (!strcmp(apple_frontend_settings.orientations, "landscape"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskLandscape;
   else if (!strcmp(apple_frontend_settings.orientations, "portrait"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;

   /* Set bluetooth mode */
   small_keyboard = !(strcmp(apple_frontend_settings.bluetooth_mode, "small_keyboard"));
   is_icade       = !(strcmp(apple_frontend_settings.bluetooth_mode, "icade"));
   is_btstack     = !(strcmp(apple_frontend_settings.bluetooth_mode, "btstack"));
       
   apple_input_enable_small_keyboard(small_keyboard);
   apple_input_enable_icade(is_icade);
   btstack_set_poweron(is_btstack);
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

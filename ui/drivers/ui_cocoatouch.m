/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include <file/file_path.h>

#include "cocoa/cocoa_common.h"
#include "../ui_companion_driver.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../retroarch.h"
#import <AVFoundation/AVFoundation.h>
#include "../../frontend/frontend.h"
#include "../../runloop.h"

#ifdef HAVE_MENU
#include "../../menu/menu_setting.h"
#endif

static char msg_old[PATH_MAX_LENGTH];
static id apple_platform;
static CFRunLoopObserverRef iterate_observer;

/* forward declaration */
void apple_rarch_exited(void);

static void rarch_enable_ui(void)
{
   AVAudioSession *audioSession = [AVAudioSession sharedInstance];
   [audioSession setCategory: AVAudioSessionCategoryAmbient error: nil];
   [audioSession setActive:YES error:nil];
    
   bool boolean = true;

   ui_companion_set_foreground(true);

   runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
   runloop_ctl(RUNLOOP_CTL_SET_IDLE,   &boolean);
   rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
}

static void rarch_disable_ui(void)
{
   bool boolean = false;

   ui_companion_set_foreground(false);

   runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
   runloop_ctl(RUNLOOP_CTL_SET_IDLE,   &boolean);
   rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
   AVAudioSession *audioSession = [AVAudioSession sharedInstance];
   [audioSession setCategory: AVAudioSessionCategoryAmbient error: nil];
   [audioSession setActive:YES error:nil];
}

static void ui_companion_cocoatouch_event_command(
      void *data, enum event_command cmd)
{
    (void)data;
    event_command(cmd);
}

static void rarch_draw_observer(CFRunLoopObserverRef observer,
    CFRunLoopActivity activity, void *info)
{
   unsigned sleep_ms  = 0;
   int ret            = runloop_iterate(&sleep_ms);

   if (ret == 1 && !ui_companion_is_on_foreground() && sleep_ms > 0)
      retro_sleep(sleep_ms);
   runloop_ctl(RUNLOOP_CTL_DATA_ITERATE, NULL);

   if (ret == -1)
   {
      ui_companion_cocoatouch_event_command(NULL, EVENT_CMD_MENU_SAVE_CURRENT_CONFIG);
      main_exit(NULL);
      return;
   }

   if (runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
      return;
   CFRunLoopWakeUp(CFRunLoopGetMain());
}

apple_frontend_settings_t apple_frontend_settings;

void get_ios_version(int *major, int *minor)
{
    NSArray *decomposed_os_version = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];
    
    if (major && decomposed_os_version.count > 0)
        *major = [decomposed_os_version[0] integerValue];
    if (minor && decomposed_os_version.count > 1)
        *minor = [decomposed_os_version[1] integerValue];
}

extern float cocoagl_gfx_ctx_get_native_scale(void);

/* Input helpers: This is kept here because it needs ObjC */
static void handle_touch_event(NSArray* touches)
{
   unsigned i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();
   float scale               = cocoagl_gfx_ctx_get_native_scale();

   if (!apple)
      return;

   apple->touch_count = 0;
   
   for (i = 0; i < touches.count && (apple->touch_count < MAX_TOUCHES); i++)
   {
      CGPoint       coord;
      UITouch      *touch = [touches objectAtIndex:i];
      
      if (touch.view != [CocoaView get].view)
         continue;

      coord = [touch locationInView:[touch view]];

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
- (void)handleKeyUIEvent:(UIEvent*)event;
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

// This is specifically for iOS 9, according to the private headers
-(void)handleKeyUIEvent:(UIEvent *)event {
    /* This gets called twice with the same timestamp
     * for each keypress, that's fine for polling
     * but is bad for business with events. */
    static double last_time_stamp;
    
    if (last_time_stamp == event.timestamp) {
        return [super handleKeyUIEvent:event];
    }
    
    last_time_stamp = event.timestamp;
    
    /* If the _hidEvent is null, [event _keyCode] will crash.
     * (This happens with the on screen keyboard). */
    if (event._hidEvent)
    {
        NSString       *ch = (NSString*)event._privateInput;
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
            unsigned i;
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
    
    [super handleKeyUIEvent:event];
}

// This is for iOS versions < 9.0
- (id)_keyCommandForEvent:(UIEvent*)event
{
   /* This gets called twice with the same timestamp 
    * for each keypress, that's fine for polling
    * but is bad for business with events. */
   static double last_time_stamp;
   
   if (last_time_stamp == event.timestamp)
      return [super _keyCommandForEvent:event];
   last_time_stamp = event.timestamp;
   
   /* If the _hidEvent is null, [event _keyCode] will crash. 
    * (This happens with the on screen keyboard). */
   if (event._hidEvent)
   {
      NSString       *ch = (NSString*)event._privateInput;
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
         unsigned i;
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
   int major, minor;
   [super sendEvent:event];

   if (event.allTouches.count)
      handle_touch_event(event.allTouches.allObjects);

   get_ios_version(&major, &minor);
    
   if ((major < 7) && [event respondsToSelector:@selector(_gsEvent)])
   {
      /* Keyboard event hack for iOS versions prior to iOS 7.
       *
       * Derived from: 
       * http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
       */
      const uint8_t *eventMem = objc_unretainedPointer([event performSelector:@selector(_gsEvent)]);
      int           eventType = eventMem ? *(int*)&eventMem[8] : 0;

      switch (eventType)
      {
         case GSEVENT_TYPE_KEYDOWN:
         case GSEVENT_TYPE_KEYUP:
            apple_input_keyboard_event(eventType == GSEVENT_TYPE_KEYDOWN,
                  *(uint16_t*)&eventMem[0x3C], 0, 0, RETRO_DEVICE_KEYBOARD);
            break;
      }
   }
}

@end

@implementation RetroArch_iOS

+ (RetroArch_iOS*)get
{
   // implicitly initializes your audio session
   AVAudioSession *audioSession = [AVAudioSession sharedInstance];
   [audioSession setCategory: AVAudioSessionCategoryAmbient error: nil];
   [audioSession setActive:YES error:nil];
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   apple_platform   = self;

   [self setDelegate:self];
    
   if (rarch_main(0, NULL, NULL))
       apple_rarch_exited();
    /* Other background audio check */
   [self supportOtherAudioSessions];
   /* Setup window */
   self.window      = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self.window makeKeyAndVisible];

   self.mainmenu = [RAMainMenu new];
   self.mainmenu.last_menu = self.mainmenu;
   [self pushViewController:self.mainmenu animated:NO];

   [self refreshSystemConfig];
   [self showGameView];
   [self supportOtherAudioSessions];

   if (rarch_main(0, NULL, NULL))
      apple_rarch_exited();

  iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting,
                                             true, 0, rarch_draw_observer, 0);
  CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer, kCFRunLoopCommonModes);
    
#ifdef HAVE_MFI
    extern bool apple_gamecontroller_joypad_init(void *data);
    apple_gamecontroller_joypad_init(NULL);
#endif
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [self supportOtherAudioSessions];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
   CFRunLoopObserverInvalidate(iterate_observer);
   CFRelease(iterate_observer);
   iterate_observer = NULL;
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   settings_t *settings = config_get_ptr();
   
   [self supportOtherAudioSessions];
   if (settings->ui.companion_start_on_boot)
      return;
    
  [self showGameView];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   [self supportOtherAudioSessions];
   dispatch_async(dispatch_get_main_queue(),
                  ^{
                  ui_companion_cocoatouch_event_command(NULL, EVENT_CMD_MENU_SAVE_CURRENT_CONFIG);
                  });
   [self showPauseMenu: self];
}

-(BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
   NSString *filename = (NSString*)url.path.lastPathComponent;
   NSError     *error = nil;
  [self supportOtherAudioSessions];
    
   [[NSFileManager defaultManager] moveItemAtPath:[url path] toPath:[self.documentsDirectory stringByAppendingPathComponent:filename] error:&error];
   
   if (error)
      printf("%s\n", [[error description] UTF8String]);
   
   return true;
}

- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
   [self setToolbarHidden:![[viewController toolbarItems] count] animated:YES];
   [self refreshSystemConfig];
}

- (void)showGameView
{
    // implicitly initializes your audio session
   [self supportOtherAudioSessions];
   [self popToRootViewControllerAnimated:NO];
   [self setToolbarHidden:true animated:NO];
   [[UIApplication sharedApplication] setStatusBarHidden:true withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:true];
   [self.window setRootViewController:[CocoaView get]];

   ui_companion_cocoatouch_event_command(NULL, EVENT_CMD_AUDIO_START);
   rarch_disable_ui();
}

- (IBAction)showPauseMenu:(id)sender
{
   //ui_companion_cocoatouch_event_command(NULL, EVENT_CMD_AUDIO_STOP);
   rarch_enable_ui();

   [[UIApplication sharedApplication] setStatusBarHidden:false withAnimation:UIStatusBarAnimationNone];
   [[UIApplication sharedApplication] setIdleTimerDisabled:false];
   [self.window setRootViewController:self];
}


- (void)toggleUI
{
   if (ui_companion_is_on_foreground())
   {
      [self showGameView];
   }
   else
   {
      [self showPauseMenu:self];
   }
   [self supportOtherAudioSessions];
}

- (void)refreshSystemConfig
{
   /* Get enabled orientations */
   apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskAll;
   
   if (!strcmp(apple_frontend_settings.orientations, "landscape"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskLandscape;
   else if (!strcmp(apple_frontend_settings.orientations, "portrait"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
}

- (void)mainMenuRefresh 
{
  [self.mainmenu reloadData];
}

- (void)mainMenuPushPop: (bool)pushp
{
  if ( pushp ) {
    self.menu_count++;
    RAMenuBase* next_menu = [RAMainMenu new];
    next_menu.last_menu = self.mainmenu;
    self.mainmenu = next_menu;
    [self pushViewController:self.mainmenu animated:YES];
  } else {
    if ( self.menu_count == 0 ) {
      [self.mainmenu reloadData];
    } else {
      self.menu_count--;

      [self popViewControllerAnimated:YES];
      self.mainmenu = self.mainmenu.last_menu;      
    }
  }
}

- (void)supportOtherAudioSessions
{
    // implicitly initializes your audio session
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    [audioSession setCategory: AVAudioSessionCategoryAmbient error: nil];
    [audioSession setActive:YES error:nil];
}

- (void)mainMenuRenderMessageBox:(NSString *)msg
{
  [self.mainmenu renderMessageBox:msg];
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

void apple_display_alert(const char *message, const char *title)
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:BOXSTRING(title)
                                             message:BOXSTRING(message)
                                             delegate:nil
                                             cancelButtonTitle:BOXSTRING("OK")
                                             otherButtonTitles:nil];
   [alert show];
}

void apple_rarch_exited(void)
{
    RetroArch_iOS *ap = (RetroArch_iOS *)apple_platform;
    
    if (!ap)
        return;
    [ap supportOtherAudioSessions];
    [ap showPauseMenu:ap];
}

typedef struct ui_companion_cocoatouch
{
   void *empty;
} ui_companion_cocoatouch_t;

static void ui_companion_cocoatouch_notify_content_loaded(void *data)
{
   RetroArch_iOS *ap = (RetroArch_iOS *)apple_platform;

   (void)data;

   if (ap)
      [ap showGameView];
}

static void ui_companion_cocoatouch_toggle(void *data)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   (void)data;

   if (ap)
      [ap toggleUI];
}

static int ui_companion_cocoatouch_iterate(void *data, unsigned action)
{
   RetroArch_iOS *ap  = (RetroArch_iOS*)apple_platform;

   (void)data;
    
   if (ap)
      [ap showPauseMenu:ap];

   return 0;
}

static void ui_companion_cocoatouch_deinit(void *data)
{
   ui_companion_cocoatouch_t *handle = (ui_companion_cocoatouch_t*)data;

   apple_rarch_exited();

   if (handle)
      free(handle);
}

static void *ui_companion_cocoatouch_init(void)
{
   ui_companion_cocoatouch_t *handle = (ui_companion_cocoatouch_t*)
    calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;
    
   rarch_enable_ui();

   return handle;
}

static size_t old_size = 0;

static void ui_companion_cocoatouch_notify_list_pushed(void *data,
   file_list_t *list, file_list_t *menu_list)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;
   bool pushp = false;

   size_t new_size = file_list_get_size( menu_list );

   /* FIXME workaround for the double call */
   if ( old_size == 0 )
   {
      old_size = new_size;
      return;
   }

   if ( old_size == new_size ) {
     pushp = false;
   } else if ( old_size < new_size ) {
     pushp = true;
   } else if ( old_size > new_size ) {
     printf( "notify_list_pushed: old size should not be larger\n" );
   }
   old_size = new_size;
      
   if (ap)
     [ap mainMenuPushPop: pushp];
}

static void ui_companion_cocoatouch_notify_refresh(void *data)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   if (ap)
     [ap mainMenuRefresh];
}

static void ui_companion_cocoatouch_render_messagebox(const char *msg)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   if (ap && strcmp(msg, msg_old))
   {
      [ap mainMenuRenderMessageBox: [NSString stringWithUTF8String:msg]];
      strlcpy(msg_old, msg, sizeof(msg_old));
   }
}

static void ui_companion_cocoatouch_msg_queue_push(const char *msg,
   unsigned priority, unsigned duration, bool flush)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   if (ap && msg)
   {
      [ap.mainmenu msgQueuePush: [NSString stringWithUTF8String:msg]];
   }
}

const ui_companion_driver_t ui_companion_cocoatouch = {
   ui_companion_cocoatouch_init,
   ui_companion_cocoatouch_deinit,
   ui_companion_cocoatouch_iterate,
   ui_companion_cocoatouch_toggle,
   ui_companion_cocoatouch_event_command,
   ui_companion_cocoatouch_notify_content_loaded,
   ui_companion_cocoatouch_notify_list_pushed,
   ui_companion_cocoatouch_notify_refresh,
   ui_companion_cocoatouch_msg_queue_push,
   ui_companion_cocoatouch_render_messagebox,
   "cocoatouch",
};

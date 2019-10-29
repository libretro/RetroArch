/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <retro_timers.h>

#include "cocoa/cocoa_common.h"
#include "../ui_companion_driver.h"
#include "../../configuration.h"
#include "../../frontend/frontend.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../retroarch.h"

#ifdef HAVE_MENU
#include "../../menu/menu_setting.h"
#endif

static char msg_old[PATH_MAX_LENGTH];
#ifdef HAVE_COCOA_METAL
id<ApplePlatform> apple_platform;
#else
static id apple_platform;
#endif
static CFRunLoopObserverRef iterate_observer;

/* forward declaration */
static void apple_rarch_exited(void);

static void rarch_enable_ui(void)
{
   bool boolean = true;

   ui_companion_set_foreground(true);

   rarch_ctl(RARCH_CTL_SET_PAUSED, &boolean);
   rarch_ctl(RARCH_CTL_SET_IDLE,   &boolean);
   retroarch_menu_running();
}

static void rarch_disable_ui(void)
{
   bool boolean = false;

   ui_companion_set_foreground(false);

   rarch_ctl(RARCH_CTL_SET_PAUSED, &boolean);
   rarch_ctl(RARCH_CTL_SET_IDLE,   &boolean);
   retroarch_menu_running_finished(false);
}

static void ui_companion_cocoatouch_event_command(
      void *data, enum event_command cmd)
{
    (void)data;
}

static void rarch_draw_observer(CFRunLoopObserverRef observer,
    CFRunLoopActivity activity, void *info)
{
   int          ret   = runloop_iterate();

   task_queue_check();

   if (ret == -1)
   {
      ui_companion_cocoatouch_event_command(NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
      main_exit(NULL);
      return;
   }

   if (rarch_ctl(RARCH_CTL_IS_IDLE, NULL))
      return;
   CFRunLoopWakeUp(CFRunLoopGetMain());
}

apple_frontend_settings_t apple_frontend_settings;

void get_ios_version(int *major, int *minor)
{
    NSArray *decomposed_os_version = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];

    if (major && decomposed_os_version.count > 0)
        *major = (int)[decomposed_os_version[0] integerValue];
    if (minor && decomposed_os_version.count > 1)
        *minor = (int)[decomposed_os_version[1] integerValue];
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

#ifndef HAVE_APPLE_STORE
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
#endif

@interface RApplication : UIApplication
@end

@implementation RApplication

#ifndef HAVE_APPLE_STORE
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

    if (last_time_stamp == event.timestamp)
       return [super handleKeyUIEvent:event];

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
#endif

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

- (void)sendEvent:(UIEvent *)event
{
   int major, minor;
   [super sendEvent:event];

   if (event.allTouches.count)
      handle_touch_event(event.allTouches.allObjects);

   get_ios_version(&major, &minor);

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 70000
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
#endif
}

@end

@implementation RetroArch_iOS

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

-(NSString*)documentsDirectory {
    if ( _documentsDirectory == nil ) {
#if TARGET_OS_IOS
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
#elif TARGET_OS_TV
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
#endif

        _documentsDirectory = paths.firstObject;
    }
    return _documentsDirectory;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   char arguments[]   = "retroarch";
   char       *argv[] = {arguments,   NULL};
   int argc           = 1;
   apple_platform     = self;

   [self setDelegate:self];

   /* Setup window */
   self.window      = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self.window makeKeyAndVisible];

#if TARGET_OS_IOS
   self.mainmenu = [RAMainMenu new];
   self.mainmenu.last_menu = self.mainmenu;
   [self pushViewController:self.mainmenu animated:NO];
#endif

   [self refreshSystemConfig];
   [self showGameView];

   if (rarch_main(argc, argv, NULL))
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

   if (settings->bools.ui_companion_start_on_boot)
      return;

  [self showGameView];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   dispatch_async(dispatch_get_main_queue(),
                  ^{
                  ui_companion_cocoatouch_event_command(NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
                  });
}

-(BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
   NSString *filename = (NSString*)url.path.lastPathComponent;
   NSError     *error = nil;

   [[NSFileManager defaultManager] moveItemAtPath:[url path] toPath:[self.documentsDirectory stringByAppendingPathComponent:filename] error:&error];

   if (error)
      printf("%s\n", [[error description] UTF8String]);

   return true;
}

- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
#if TARGET_OS_IOS
   [self setToolbarHidden:![[viewController toolbarItems] count] animated:YES];
#endif
   [self refreshSystemConfig];
}

- (void)showGameView
{
    [self popToRootViewControllerAnimated:NO];

#if TARGET_OS_IOS
   [self setToolbarHidden:true animated:NO];
   [[UIApplication sharedApplication] setStatusBarHidden:true withAnimation:UIStatusBarAnimationNone];
#endif

    [[UIApplication sharedApplication] setIdleTimerDisabled:true];
   [self.window setRootViewController:[CocoaView get]];

   ui_companion_cocoatouch_event_command(NULL, CMD_EVENT_AUDIO_START);
   rarch_disable_ui();
}

- (IBAction)showPauseMenu:(id)sender
{
   rarch_enable_ui();

#if TARGET_OS_IOS
   [[UIApplication sharedApplication] setStatusBarHidden:false withAnimation:UIStatusBarAnimationNone];
#endif

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
}

- (void)refreshSystemConfig
{
#if TARGET_OS_IOS
   /* Get enabled orientations */
   apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskAll;

   if (string_is_equal(apple_frontend_settings.orientations, "landscape"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskLandscape;
   else if (string_is_equal(apple_frontend_settings.orientations, "portrait"))
      apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskPortrait
         | UIInterfaceOrientationMaskPortraitUpsideDown;
#endif
}

- (void)mainMenuRefresh
{
#if TARGET_OS_IOS
  [self.mainmenu reloadData];
#endif
}

- (void)mainMenuPushPop: (bool)pushp
{
#if TARGET_OS_IOS
  if ( pushp )
  {
     self.menu_count++;
     RAMenuBase* next_menu = [RAMainMenu new];
     next_menu.last_menu = self.mainmenu;
     self.mainmenu = next_menu;
     [self pushViewController:self.mainmenu animated:YES];
  }
  else
  {
     if ( self.menu_count == 0 )
        [self.mainmenu reloadData];
     else
     {
        self.menu_count--;

        [self popViewControllerAnimated:YES];
        self.mainmenu = self.mainmenu.last_menu;
     }
  }
#endif
}

- (void)supportOtherAudioSessions
{
}

- (void)mainMenuRenderMessageBox:(NSString *)msg
{
#if TARGET_OS_IOS
  [self.mainmenu renderMessageBox:msg];
#endif
}

@end

int main(int argc, char *argv[])
{
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

#if 0
static void apple_display_alert(const char *message, const char *title)
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:BOXSTRING(title)
                                             message:BOXSTRING(message)
                                             delegate:nil
                                             cancelButtonTitle:BOXSTRING("OK")
                                             otherButtonTitles:nil];
   [alert show];
}
#endif

static void apple_rarch_exited(void)
{
    RetroArch_iOS *ap = (RetroArch_iOS *)apple_platform;

    if (!ap)
        return;
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

static void ui_companion_cocoatouch_toggle(void *data, bool force)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   (void)data;

   if (ap)
      [ap toggleUI];
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
   bool pushp          = false;
   size_t new_size     = file_list_get_size( menu_list );

   /* FIXME workaround for the double call */
   if ( old_size == 0 )
   {
      old_size = new_size;
      return;
   }

   if ( old_size == new_size )
     pushp = false;
   else if ( old_size < new_size )
     pushp = true;
   else if ( old_size > new_size )
     printf( "notify_list_pushed: old size should not be larger\n" );

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

   if (ap && !string_is_equal(msg, msg_old))
   {
      [ap mainMenuRenderMessageBox: [NSString stringWithUTF8String:msg]];
      strlcpy(msg_old, msg, sizeof(msg_old));
   }
}

static void ui_companion_cocoatouch_msg_queue_push(void *data, const char *msg,
   unsigned priority, unsigned duration, bool flush)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   if (ap && msg)
   {
#if TARGET_OS_IOS
      [ap.mainmenu msgQueuePush: [NSString stringWithUTF8String:msg]];
#endif
   }
}

ui_companion_driver_t ui_companion_cocoatouch = {
   ui_companion_cocoatouch_init,
   ui_companion_cocoatouch_deinit,
   ui_companion_cocoatouch_toggle,
   ui_companion_cocoatouch_event_command,
   ui_companion_cocoatouch_notify_content_loaded,
   ui_companion_cocoatouch_notify_list_pushed,
   ui_companion_cocoatouch_notify_refresh,
   ui_companion_cocoatouch_msg_queue_push,
   ui_companion_cocoatouch_render_messagebox,
   NULL, /* get_main_window */
   NULL, /* log_msg */
   NULL, /* ui_browser_window_null */
   NULL, /* ui_msg_window_null */
   NULL, /* ui_window_null */
   NULL, /* ui_application_null */
   "cocoatouch",
};

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
#include "cocoa/apple_platform.h"
#include "../ui_companion_driver.h"
#include "../../audio/audio_driver.h"
#include "../../configuration.h"
#include "../../frontend/frontend.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef HAVE_MENU
#include "../../menu/menu_setting.h"
#endif

#import <AVFoundation/AVFoundation.h>

#import <MetricKit/MetricKit.h>
#import <MetricKit/MXMetricManager.h>

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
#import "JITSupport.h"
id<ApplePlatform> apple_platform;
#else
static id apple_platform;
#endif
static CFRunLoopObserverRef iterate_observer;

static void ui_companion_cocoatouch_event_command(
      void *data, enum event_command cmd) { }

static void rarch_draw_observer(CFRunLoopObserverRef observer,
    CFRunLoopActivity activity, void *info)
{
   uint32_t runloop_flags;
   int          ret   = runloop_iterate();

   task_queue_check();

   if (ret == -1)
   {
      ui_companion_cocoatouch_event_command(
            NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
      main_exit(NULL);
      exit(0);
      return;
   }

   runloop_flags = runloop_get_flags();
   if (!(runloop_flags & RUNLOOP_FLAG_IDLE))
      CFRunLoopWakeUp(CFRunLoopGetMain());
}

void rarch_start_draw_observer(void)
{
   if (iterate_observer && CFRunLoopObserverIsValid(iterate_observer))
       return;

   if (iterate_observer != NULL)
      CFRelease(iterate_observer);
   iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting,
                                              true, 0, rarch_draw_observer, 0);
   CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer, kCFRunLoopCommonModes);
}

void rarch_stop_draw_observer(void)
{
    if (!iterate_observer || !CFRunLoopObserverIsValid(iterate_observer))
        return;
    CFRunLoopObserverInvalidate(iterate_observer);
    CFRelease(iterate_observer);
    iterate_observer = NULL;
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

/* Input helpers: This is kept here because it needs ObjC */
static void handle_touch_event(NSArray* touches)
{
#if !TARGET_OS_TV
   unsigned i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)
      input_state_get_ptr()->current_data;
   float scale               = cocoa_screen_get_native_scale();

   if (!apple)
      return;

   apple->touch_count = 0;

   for (i = 0; i < touches.count && (apple->touch_count < MAX_TOUCHES); i++)
   {
      UITouch      *touch = [touches objectAtIndex:i];
      CGPoint       coord = [touch locationInView:[touch view]];
      if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
      {
         apple->touches[apple->touch_count   ].screen_x = coord.x * scale;
         apple->touches[apple->touch_count ++].screen_y = coord.y * scale;
      }
   }
#endif
}

#ifndef HAVE_APPLE_STORE
/* iOS7 Keyboard support */
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
   NSAlphaShiftKeyMask                  = 1 << 16,
   NSShiftKeyMask                       = 1 << 17,
   NSControlKeyMask                     = 1 << 18,
   NSAlternateKeyMask                   = 1 << 19,
   NSCommandKeyMask                     = 1 << 20,
   NSNumericPadKeyMask                  = 1 << 21,
   NSHelpKeyMask                        = 1 << 22,
   NSFunctionKeyMask                    = 1 << 23,
   NSDeviceIndependentModifierFlagsMask = 0xffff0000U
};

/* This is specifically for iOS 9, according to the private headers */
-(void)handleKeyUIEvent:(UIEvent *)event
{
    /* This gets called twice with the same timestamp
     * for each keypress, that's fine for polling
     * but is bad for business with events. */
    static double last_time_stamp;

    if (last_time_stamp == event.timestamp)
       return [super handleKeyUIEvent:event];

    last_time_stamp        = event.timestamp;

    /* If the _hidEvent is NULL, [event _keyCode] will crash.
     * (This happens with the on screen keyboard). */
    if (event._hidEvent)
    {
        NSString       *ch = (NSString*)event._privateInput;
        uint32_t character = 0;
        uint32_t mod       = 0;
        NSUInteger mods    = event._modifierFlags;

        if (mods & NSAlphaShiftKeyMask)
           mod |= RETROKMOD_CAPSLOCK;
        if (mods & NSShiftKeyMask)
           mod |= RETROKMOD_SHIFT;
        if (mods & NSControlKeyMask)
           mod |= RETROKMOD_CTRL;
        if (mods & NSAlternateKeyMask)
           mod |= RETROKMOD_ALT;
        if (mods & NSCommandKeyMask)
           mod |= RETROKMOD_META;
        if (mods & NSNumericPadKeyMask)
           mod |= RETROKMOD_NUMLOCK;

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

/* This is for iOS versions < 9.0 */
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
      NSUInteger mods    = event._modifierFlags;

      if (mods & NSAlphaShiftKeyMask)
         mod |= RETROKMOD_CAPSLOCK;
      if (mods & NSShiftKeyMask)
         mod |= RETROKMOD_SHIFT;
      if (mods & NSControlKeyMask)
         mod |= RETROKMOD_CTRL;
      if (mods & NSAlternateKeyMask)
         mod |= RETROKMOD_ALT;
      if (mods & NSCommandKeyMask)
         mod |= RETROKMOD_META;
      if (mods & NSNumericPadKeyMask)
         mod |= RETROKMOD_NUMLOCK;

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
   [super sendEvent:event];
    if (@available(iOS 13.4, tvOS 13.4, *)) {
        if (event.type == UIEventTypeHover)
            return;
    }
   if (event.allTouches.count)
      handle_touch_event(event.allTouches.allObjects);

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 70000
   {
      int major, minor;
      get_ios_version(&major, &minor);

      if ((major < 7) && [event respondsToSelector:@selector(_gsEvent)])
      {
         /* Keyboard event hack for iOS versions prior to iOS 7.
          *
          * Derived from:
	  * http://nacho4d-nacho4d.blogspot.com/2012/01/
	  * catching-keyboard-events-in-ios.html
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
#endif
}

@end

#if TARGET_OS_IOS
@interface RetroArch_iOS () <MXMetricManagerSubscriber>

@end
#endif

@implementation RetroArch_iOS

#pragma mark - ApplePlatform
-(id)renderView { return _renderView; }
-(bool)hasFocus
{
    return [[UIApplication sharedApplication] applicationState] == UIApplicationStateActive;
}

- (void)setViewType:(apple_view_type_t)vt
{
   if (vt == _vt)
      return;

   _vt = vt;
   if (_renderView != nil)
   {
      [_renderView removeFromSuperview];
      _renderView = nil;
   }

   switch (vt)
   {
#ifdef HAVE_COCOA_METAL
       case APPLE_VIEW_TYPE_VULKAN:
       case APPLE_VIEW_TYPE_METAL:
         {
            MetalView *v = [MetalView new];
            v.paused                = YES;
            v.enableSetNeedsDisplay = NO;
#if TARGET_OS_IOS
            v.multipleTouchEnabled  = YES;
#endif
            _renderView = v;
         }
         break;
#endif
       case APPLE_VIEW_TYPE_OPENGL_ES:
         _renderView = (BRIDGE GLKView*)glkitview_init();
         break;

       case APPLE_VIEW_TYPE_NONE:
       default:
         return;
   }

   _renderView.translatesAutoresizingMaskIntoConstraints = NO;
   UIView *rootView = [CocoaView get].view;
   [rootView addSubview:_renderView];
   [[_renderView.topAnchor constraintEqualToAnchor:rootView.topAnchor] setActive:YES];
   [[_renderView.bottomAnchor constraintEqualToAnchor:rootView.bottomAnchor] setActive:YES];
   [[_renderView.leadingAnchor constraintEqualToAnchor:rootView.leadingAnchor] setActive:YES];
   [[_renderView.trailingAnchor constraintEqualToAnchor:rootView.trailingAnchor] setActive:YES];
   [_renderView layoutIfNeeded];
}

- (apple_view_type_t)viewType { return _vt; }

- (void)setVideoMode:(gfx_ctx_mode_t)mode
{
#ifdef HAVE_COCOA_METAL
   MetalView *metalView = (MetalView*) _renderView;
   CGFloat scale        = [[UIScreen mainScreen] scale];
   [metalView setDrawableSize:CGSizeMake(
         _renderView.bounds.size.width * scale,
         _renderView.bounds.size.height * scale
         )];
#endif
}

- (void)setCursorVisible:(bool)v { /* no-op for iOS */ }
- (bool)setDisableDisplaySleep:(bool)disable
{
#if TARGET_OS_TV
   [[UIApplication sharedApplication] setIdleTimerDisabled:disable];
   return YES;
#else
   return NO;
#endif
}
+ (RetroArch_iOS*)get { return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate]; }

-(NSString*)documentsDirectory
{
   if (_documentsDirectory == nil)
   {
#if TARGET_OS_IOS
      NSArray *paths      = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
#elif TARGET_OS_TV
      NSArray *paths      = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
#endif
      _documentsDirectory = paths.firstObject;
   }
   return _documentsDirectory;
}

- (void)handleAudioSessionInterruption:(NSNotification *)notification
{
   NSNumber *type = notification.userInfo[AVAudioSessionInterruptionTypeKey];
   if (![type isKindOfClass:[NSNumber class]])
      return;

   if ([type unsignedIntegerValue] == AVAudioSessionInterruptionTypeBegan)
   {
      RARCH_LOG("AudioSession Interruption Began\n");
      audio_driver_stop();
   }
   else if ([type unsignedIntegerValue] == AVAudioSessionInterruptionTypeEnded)
   {
      RARCH_LOG("AudioSession Interruption Ended\n");
      audio_driver_start(false);
   }
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   NSError *error;
   char arguments[]   = "retroarch";
   char       *argv[] = {arguments,   NULL};
   int argc           = 1;
   apple_platform     = self;

   [self setDelegate:self];

   /* Setup window */
   self.window        = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self.window makeKeyAndVisible];

   [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&error];
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioSessionInterruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];

   [self refreshSystemConfig];
   [self showGameView];

   jb_start_altkit();

   rarch_main(argc, argv, NULL);

   rarch_start_draw_observer();

#if TARGET_OS_IOS
   [MXMetricManager.sharedManager addSubscriber:self];
#endif

#ifdef HAVE_MFI
   extern void *apple_gamecontroller_joypad_init(void *data);
   apple_gamecontroller_joypad_init(NULL);
#endif
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   rarch_stop_draw_observer();
}

- (void)applicationWillTerminate:(UIApplication *)application
{
   rarch_stop_draw_observer();
   retroarch_main_quit();
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   rarch_start_draw_observer();
   settings_t *settings            = config_get_ptr();
   bool ui_companion_start_on_boot = settings->bools.ui_companion_start_on_boot;

   if (!ui_companion_start_on_boot)
      [self showGameView];
}

-(BOOL)application:(UIApplication *)app openURL:(NSURL *)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
   NSFileManager *manager = [NSFileManager defaultManager];
   NSString     *filename = (NSString*)url.path.lastPathComponent;
   NSError         *error = nil;
   NSString  *destination = [self.documentsDirectory stringByAppendingPathComponent:filename];
   /* Copy file to documents directory if it's not already 
    * inside Documents directory */
   if ([url startAccessingSecurityScopedResource]) {
      if (![[url path] containsString: self.documentsDirectory])
         if (![manager fileExistsAtPath:destination])
            [manager copyItemAtPath:[url path] toPath:destination error:&error];
      [url stopAccessingSecurityScopedResource];
   }
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
   [[UIApplication sharedApplication] setIdleTimerDisabled:true];
#endif

   [self.window setRootViewController:[CocoaView get]];

   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1.0 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
         command_event(CMD_EVENT_AUDIO_START, NULL);
         });
}

- (void)refreshSystemConfig
{
#if TARGET_OS_IOS
   /* Get enabled orientations */
   apple_frontend_settings.orientation_flags = UIInterfaceOrientationMaskAll;

   if (string_is_equal(apple_frontend_settings.orientations, "landscape"))
      apple_frontend_settings.orientation_flags = 
           UIInterfaceOrientationMaskLandscape;
   else if (string_is_equal(apple_frontend_settings.orientations, "portrait"))
      apple_frontend_settings.orientation_flags = 
           UIInterfaceOrientationMaskPortrait
         | UIInterfaceOrientationMaskPortraitUpsideDown;
#endif
}

- (void)supportOtherAudioSessions { }

#if TARGET_OS_IOS
- (void)didReceiveMetricPayloads:(NSArray<MXMetricPayload *> *)payloads
{
    for (MXMetricPayload *payload in payloads)
    {
        NSString *json = [[NSString alloc] initWithData:[payload JSONRepresentation] encoding:kCFStringEncodingUTF8];
        RARCH_LOG("Got Metric Payload:\n%s\n", [json cStringUsingEncoding:kCFStringEncodingUTF8]);
    }
}

- (void)didReceiveDiagnosticPayloads:(NSArray<MXDiagnosticPayload *> *)payloads
{
    for (MXDiagnosticPayload *payload in payloads)
    {
        NSString *json = [[NSString alloc] initWithData:[payload JSONRepresentation] encoding:kCFStringEncodingUTF8];
        RARCH_LOG("Got Diagnostic Payload:\n%s\n", [json cStringUsingEncoding:kCFStringEncodingUTF8]);
    }
}
#endif

@end

int main(int argc, char *argv[])
{
#if TARGET_OS_IOS
    if (jb_enable_ptrace_hack())
        RARCH_LOG("Ptrace hack complete, JIT support is enabled.\n");
    else
        RARCH_WARN("Ptrace hack NOT available; Please use an app like Jitterbug.\n");
#endif
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

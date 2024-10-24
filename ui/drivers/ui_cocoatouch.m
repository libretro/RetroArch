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
#include "../../tasks/task_content.h"
#include "../../verbosity.h"

#ifdef HAVE_MENU
#include "../../menu/menu_setting.h"
#endif

#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>

#import <MetricKit/MetricKit.h>
#import <MetricKit/MXMetricManager.h>

#ifdef HAVE_MFI
#import <GameController/GCMouse.h>
#endif

#ifdef HAVE_SDL2
#define SDL_MAIN_HANDLED
#include "SDL.h"
#endif

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
#import "JITSupport.h"
id<ApplePlatform> apple_platform;
#else
static id apple_platform;
#endif
static CFRunLoopObserverRef iterate_observer;

static void ui_companion_cocoatouch_event_command(
      void *data, enum event_command cmd) { }

static struct string_list *ui_companion_cocoatouch_get_app_icons(void)
{
   static struct string_list *list = NULL;
   static dispatch_once_t onceToken;

   dispatch_once(&onceToken, ^{
         union string_list_elem_attr attr;
         attr.i = 0;
         NSDictionary *iconfiles = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleIcons"];
         NSString *primary;
         const char *cstr;
#if TARGET_OS_TV
         primary = iconfiles[@"CFBundlePrimaryIcon"];
#else
         primary = iconfiles[@"CFBundlePrimaryIcon"][@"CFBundleIconName"];
#endif
         list = string_list_new();
         cstr = [primary cStringUsingEncoding:kCFStringEncodingUTF8];
         if (cstr)
            string_list_append(list, cstr, attr);

         NSArray<NSString *> *alts;
#if TARGET_OS_TV
         alts = iconfiles[@"CFBundleAlternateIcons"];
#else
         alts = [iconfiles[@"CFBundleAlternateIcons"] allKeys];
#endif
         NSArray<NSString *> *sorted = [alts sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
         for (NSString *str in sorted)
         {
            cstr = [str cStringUsingEncoding:kCFStringEncodingUTF8];
            if (cstr)
               string_list_append(list, cstr, attr);
         }
      });

   return list;
}

static void ui_companion_cocoatouch_set_app_icon(const char *iconName)
{
   NSString *str;
   if (!string_is_equal(iconName, "Default"))
      str = [NSString stringWithCString:iconName encoding:NSUTF8StringEncoding];
   [[UIApplication sharedApplication] setAlternateIconName:str completionHandler:nil];
}

static uintptr_t ui_companion_cocoatouch_get_app_icon_texture(const char *icon)
{
   static NSMutableDictionary<NSString *, NSNumber *> *textures = nil;
   static dispatch_once_t once;
   dispatch_once(&once, ^{
      textures = [NSMutableDictionary dictionaryWithCapacity:6];
   });

   NSString *iconName = [NSString stringWithUTF8String:icon];
   if (!textures[iconName])
   {
      UIImage *img = [UIImage imageNamed:iconName];
      if (!img)
      {
         RARCH_LOG("could not load %s\n", icon);
         return 0;
      }
      NSData *png = UIImagePNGRepresentation(img);
      if (!png)
      {
         RARCH_LOG("could not get png for %s\n", icon);
         return 0;
      }

      uintptr_t item;
      gfx_display_reset_textures_list_buffer(&item, TEXTURE_FILTER_MIPMAP_LINEAR,
                                             (void*)[png bytes], (unsigned int)[png length], IMAGE_TYPE_PNG,
                                             NULL, NULL);
      textures[iconName] = [NSNumber numberWithUnsignedLong:item];
   }

   return [textures[iconName] unsignedLongValue];
}

static void rarch_draw_observer(CFRunLoopObserverRef observer,
    CFRunLoopActivity activity, void *info)
{
   uint32_t runloop_flags;
   int          ret   = runloop_iterate();

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
   static int savedMajor, savedMinor;
   static dispatch_once_t onceToken;

   dispatch_once(&onceToken, ^ {
         NSArray *decomposed_os_version = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];
         if (decomposed_os_version.count > 0)
            savedMajor = (int)[decomposed_os_version[0] integerValue];
         if (decomposed_os_version.count > 1)
            savedMinor = (int)[decomposed_os_version[1] integerValue];
      });
   if (major) *major = savedMajor;
   if (minor) *minor = savedMinor;
}

bool ios_running_on_ipad(void)
{
   return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);
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
#else
- (void)handleUIPress:(UIPress *)press withEvent:(UIPressesEvent *)event down:(BOOL)down
{
   NSString       *ch;
   uint32_t character = 0;
   uint32_t mod       = 0;
   NSUInteger mods    = 0;
   if (@available(iOS 13.4, tvOS 13.4, *))
   {
      ch = (NSString*)press.key.characters;
      mods = event.modifierFlags;
   }

   if (mods & UIKeyModifierAlphaShift)
      mod |= RETROKMOD_CAPSLOCK;
   if (mods & UIKeyModifierShift)
      mod |= RETROKMOD_SHIFT;
   if (mods & UIKeyModifierControl)
      mod |= RETROKMOD_CTRL;
   if (mods & UIKeyModifierAlternate)
      mod |= RETROKMOD_ALT;
   if (mods & UIKeyModifierCommand)
      mod |= RETROKMOD_META;
   if (mods & UIKeyModifierNumericPad)
      mod |= RETROKMOD_NUMLOCK;

   if (ch && ch.length != 0)
   {
      unsigned i;
      character = [ch characterAtIndex:0];

      apple_input_keyboard_event(down,
                                 (uint32_t)press.key.keyCode, 0, mod,
                                 RETRO_DEVICE_KEYBOARD);

      for (i = 1; i < ch.length; i++)
         apple_input_keyboard_event(down,
                                    0, [ch characterAtIndex:i], mod,
                                    RETRO_DEVICE_KEYBOARD);
   }

   if (@available(iOS 13.4, tvOS 13.4, *))
      apple_input_keyboard_event(down,
                                 (uint32_t)press.key.keyCode, character, mod,
                                 RETRO_DEVICE_KEYBOARD);
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
   for (UIPress *press in presses)
      [self handleUIPress:press withEvent:event down:YES];
   [super pressesBegan:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
   for (UIPress *press in presses)
      [self handleUIPress:press withEvent:event down:NO];
   [super pressesEnded:presses withEvent:event];
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

#ifdef HAVE_COCOA_METAL
@implementation MetalLayerView

+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (instancetype)init {
    self = [super init];
    if (self) {
        [self setupMetalLayer];
    }
    return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupMetalLayer];
    }
    return self;
}

- (CAMetalLayer *)metalLayer {
    return (CAMetalLayer *)self.layer;
}

- (void)setupMetalLayer {
    self.metalLayer.device = MTLCreateSystemDefaultDevice();
    self.metalLayer.contentsScale = [UIScreen mainScreen].scale;
    self.metalLayer.opaque = YES;
}

@end
#endif

#if TARGET_OS_IOS
@interface RetroArch_iOS () <MXMetricManagerSubscriber, UIPointerInteractionDelegate>
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
         _renderView = [MetalLayerView new];
#if TARGET_OS_IOS
         _renderView.multipleTouchEnabled = YES;
#endif
         break;
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
#if TARGET_OS_IOS
   if (@available(iOS 13.4, *))
   {
      [_renderView addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
      _renderView.userInteractionEnabled = YES;
   }
#endif
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
   char arguments[]   = "retroarch";
   char       *argv[] = {arguments,   NULL};
   int argc           = 1;
   apple_platform     = self;

   [self setDelegate:self];

   /* Setup window */
   self.window        = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   [self.window makeKeyAndVisible];

   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioSessionInterruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];

   [self refreshSystemConfig];
   [self showGameView];

   rarch_main(argc, argv, NULL);

   uico_driver_state_t *uico_st     = uico_state_get_ptr();
   rarch_setting_t *appicon_setting = menu_setting_find_enum(MENU_ENUM_LABEL_APPICON_SETTINGS);
   struct string_list *icons;
   if (               appicon_setting
		   && uico_st->drv
		   && uico_st->drv->get_app_icons
		   && (icons = uico_st->drv->get_app_icons())
		   && icons->size > 1)
   {
      int i;
      size_t len    = 0;
      char *options = NULL;
      const char *icon_name;

      appicon_setting->default_value.string = icons->elems[0].data;
      icon_name = [[application alternateIconName] cStringUsingEncoding:kCFStringEncodingUTF8]; /* need to ask uico_st for this */
      for (i = 0; i < (int)icons->size; i++)
      {
         len += strlen(icons->elems[i].data) + 1;
         if (string_is_equal(icon_name, icons->elems[i].data))
            appicon_setting->value.target.string = icons->elems[i].data;
      }
      options = (char*)calloc(len, sizeof(char));
      string_list_join_concat(options, len, icons, "|");
      if (appicon_setting->values)
         free((void*)appicon_setting->values);
      appicon_setting->values = options;
   }

   rarch_start_draw_observer();

#if TARGET_OS_TV
   update_topshelf();
#endif

#if TARGET_OS_IOS
   if (@available(iOS 13.0, *))
      [MXMetricManager.sharedManager addSubscriber:self];
#endif

#ifdef HAVE_MFI
   extern void *apple_gamecontroller_joypad_init(void *data);
   apple_gamecontroller_joypad_init(NULL);
   if (@available(macOS 11, iOS 14, tvOS 14, *))
   {
      [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification
                                                        object:nil
                                                         queue:[NSOperationQueue mainQueue]
                                                    usingBlock:^(NSNotification *note)
       {
         GCMouse *mouse = note.object;
         mouse.mouseInput.mouseMovedHandler = ^(GCMouseInput * _Nonnull mouse, float delta_x, float delta_y)
         {
            cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
            if (!apple || !apple->mouse_grabbed)
               return;
            apple->mouse_rel_x       += (int16_t)delta_x;
            apple->mouse_rel_y       -= (int16_t)delta_y;
            apple->window_pos_x      += (int16_t)delta_x;
            apple->window_pos_y      -= (int16_t)delta_y;
         };
      }];
   }
#endif
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
#if TARGET_OS_TV
   update_topshelf();
#endif
   rarch_stop_draw_observer();
   command_event(CMD_EVENT_SAVE_FILES, NULL);
}

- (void)applicationWillTerminate:(UIApplication *)application
{
   rarch_stop_draw_observer();
   retroarch_main_quit();
}

- (void)applicationWillResignActive:(UIApplication *)application
{
   self.bgDate = [NSDate date];
   rarch_stop_draw_observer();
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   rarch_start_draw_observer();
   NSError *error;
   settings_t *settings            = config_get_ptr();
   bool ui_companion_start_on_boot = settings->bools.ui_companion_start_on_boot;

   if (settings->bools.audio_respect_silent_mode)
       [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&error];
   else
       [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback error:&error];

   if (!ui_companion_start_on_boot)
      [self showGameView];

#ifdef HAVE_CLOUDSYNC
   if (self.bgDate)
   {
      if (   [[NSDate date] timeIntervalSinceDate:self.bgDate] > 60.0f
          && (   !(runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING)
              || retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)))
         task_push_cloud_sync();
      self.bgDate = nil;
   }
#endif
}

-(BOOL)openRetroArchURL:(NSURL *)url
{
   if ([url.host isEqualToString:@"topshelf"])
   {
      NSURLComponents *comp = [[NSURLComponents alloc] initWithURL:url resolvingAgainstBaseURL:NO];
      NSString *ns_path, *ns_core_path;
      char path[PATH_MAX_LENGTH];
      char core_path[PATH_MAX_LENGTH];
      content_ctx_info_t content_info = { 0 };
      for (NSURLQueryItem *q in comp.queryItems)
      {
         if ([q.name isEqualToString:@"path"])
            ns_path = q.value;
         else if ([q.name isEqualToString:@"core_path"])
            ns_core_path = q.value;
      }
      if (!ns_path || !ns_core_path)
         return NO;
      fill_pathname_expand_special(path, [ns_path UTF8String], sizeof(path));
      fill_pathname_expand_special(core_path, [ns_core_path UTF8String], sizeof(core_path));
      RARCH_LOG("TopShelf told us to open %s with %s\n", path, core_path);
      return task_push_load_content_with_new_core_from_companion_ui(core_path, path,
                                                                    NULL, NULL, NULL,
                                                                    &content_info, NULL, NULL);
   }
   return NO;
}

-(BOOL)application:(UIApplication *)app openURL:(NSURL *)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
    if ([[url scheme] isEqualToString:@"retroarch"])
        return [self openRetroArchURL:url];

   NSFileManager *manager = [NSFileManager defaultManager];
   NSString     *filename = (NSString*)url.path.lastPathComponent;
   NSError         *error = nil;
   settings_t *settings   = config_get_ptr();
   char fullpath[PATH_MAX_LENGTH] = {0};
   fill_pathname_join_special(fullpath, settings->paths.directory_core_assets, [filename UTF8String], sizeof(fullpath));
   NSString  *destination = [NSString stringWithUTF8String:fullpath];
   /* Copy file to documents directory if it's not already
    * inside Documents directory */
   if ([url startAccessingSecurityScopedResource]) {
      if (![[url path] containsString: self.documentsDirectory])
         if (![manager fileExistsAtPath:destination])
            [manager copyItemAtPath:[url path] toPath:destination error:&error];
      [url stopAccessingSecurityScopedResource];
   }
   task_push_dbscan(
      settings->paths.directory_playlist,
      settings->paths.path_content_database,
      fullpath,
      false,
      false,
      NULL);
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
- (void)didReceiveMetricPayloads:(NSArray<MXMetricPayload *> *)payloads API_AVAILABLE(ios(13.0))
{
    for (MXMetricPayload *payload in payloads)
    {
        NSString *json = [[NSString alloc] initWithData:[payload JSONRepresentation] encoding:kCFStringEncodingUTF8];
        RARCH_LOG("Got Metric Payload:\n%s\n", [json cStringUsingEncoding:kCFStringEncodingUTF8]);
    }
}

- (void)didReceiveDiagnosticPayloads:(NSArray<MXDiagnosticPayload *> *)payloads API_AVAILABLE(ios(14.0))
{
    for (MXDiagnosticPayload *payload in payloads)
    {
        NSString *json = [[NSString alloc] initWithData:[payload JSONRepresentation] encoding:kCFStringEncodingUTF8];
        RARCH_LOG("Got Diagnostic Payload:\n%s\n", [json cStringUsingEncoding:kCFStringEncodingUTF8]);
    }
}

- (UIPointerStyle *)pointerInteraction:(UIPointerInteraction *)interaction styleForRegion:(UIPointerRegion *)region API_AVAILABLE(ios(13.4))
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
   if (!apple)
      return nil;
   if (apple->mouse_grabbed)
      return [UIPointerStyle hiddenPointerStyle];
   return nil;
}

- (UIPointerRegion *)pointerInteraction:(UIPointerInteraction *)interaction
                       regionForRequest:(UIPointerRegionRequest *)request
                          defaultRegion:(UIPointerRegion *)defaultRegion API_AVAILABLE(ios(13.4))
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
   if (!apple || apple->mouse_grabbed)
      return nil;
   CGPoint location = [apple_platform.renderView convertPoint:[request location] fromView:nil];
   apple->touches[0].screen_x = (int16_t)(location.x * [[UIScreen mainScreen] scale]);
   apple->touches[0].screen_y = (int16_t)(location.y * [[UIScreen mainScreen] scale]);
   apple->window_pos_x = (int16_t)(location.x * [[UIScreen mainScreen] scale]);
   apple->window_pos_y = (int16_t)(location.y * [[UIScreen mainScreen] scale]);
   return [UIPointerRegion regionWithRect:[apple_platform.renderView bounds] identifier:@"game view"];
}
#endif

@end

ui_companion_driver_t ui_companion_cocoatouch = {
   NULL, /* init */
   NULL, /* deinit */
   NULL, /* toggle */
   ui_companion_cocoatouch_event_command,
   NULL, /* notify_refresh */
   NULL, /* msg_queue_push */
   NULL, /* render_messagebox */
   NULL, /* get_main_window */
   NULL, /* log_msg */
   NULL, /* is_active */
   ui_companion_cocoatouch_get_app_icons,
   ui_companion_cocoatouch_set_app_icon,
   ui_companion_cocoatouch_get_app_icon_texture,
   NULL, /* browser_window */
   NULL, /* msg_window */
   NULL, /* window */
   NULL, /* application */
   "cocoatouch",
};

int main(int argc, char *argv[])
{
#if TARGET_OS_IOS
    if (jb_enable_ptrace_hack())
        RARCH_LOG("Ptrace hack complete, JIT support is enabled.\n");
    else
        RARCH_WARN("Ptrace hack NOT available; Please use an app like Jitterbug.\n");
#endif
#ifdef HAVE_SDL2
    SDL_SetMainReady();
#endif
   @autoreleasepool {
      return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
   }
}

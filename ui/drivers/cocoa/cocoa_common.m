/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#import <AvailabilityMacros.h>
#include <sys/stat.h>

#include "cocoa_common.h"
#include "apple_platform.h"
#include "../ui_cocoa.h"
#include <compat/apple_compat.h>

#ifdef HAVE_COCOATOUCH
#import "../../../pkg/apple/WebServer/GCDWebUploader/GCDWebUploader.h"
#import "WebServer.h"
#ifdef HAVE_IOS_SWIFT
#import "RetroArch-Swift.h"
#endif
#if TARGET_OS_TV
#import <TVServices/TVServices.h>
#import "../../pkg/apple/RetroArchTopShelfExtension/ContentProvider.h"
#endif
#if TARGET_OS_IOS
#import <MobileCoreServices/MobileCoreServices.h>
#import "../../../menu/menu_cbs.h"
#endif
#endif

#include "../../../configuration.h"
#include "../../../content.h"
#include "../../../core_info.h"
#include "../../../defaults.h"
#include "../../../frontend/frontend.h"
#include "../../../file_path_special.h"
#include "../../../menu/menu_cbs.h"
#include "../../../paths.h"
#include "../../../retroarch.h"
#include "../../../tasks/task_content.h"
#include "../../../verbosity.h"

#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#if IOS
#import <UIKit/UIAccessibility.h>
extern bool RAIsVoiceOverRunning(void)
{
   return UIAccessibilityIsVoiceOverRunning();
}
#elif OSX
#import <AppKit/AppKit.h>
extern bool RAIsVoiceOverRunning(void)
{
   if (@available(macOS 10.13, *))
      return [[NSWorkspace sharedWorkspace] isVoiceOverEnabled];
   return false;
}
#endif

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
id<ApplePlatform> apple_platform;
#else
id apple_platform;
#endif

static CocoaView* g_instance;

#ifdef HAVE_COCOATOUCH
void *glkitview_init(void);
void cocoa_file_load_with_detect_core(const char *filename);

@interface CocoaView()<GCDWebUploaderDelegate, UIGestureRecognizerDelegate
#ifdef HAVE_IOS_TOUCHMOUSE
,EmulatorTouchMouseHandlerDelegate
#endif
#if TARGET_OS_IOS
,UIDocumentPickerDelegate
#endif
>
@end
#endif

@implementation CocoaView

#if defined(OSX)
#ifdef HAVE_COCOA_METAL
- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window { return YES; }
#endif
- (void)scrollWheel:(NSEvent *)theEvent { }
#endif

#if !defined(OSX) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
-(void)step:(CADisplayLink*)target API_AVAILABLE(macos(14.0), ios(3.1), tvos(3.1))
{
#if defined(IOS)
   if ([[UIApplication sharedApplication] applicationState] != UIApplicationStateActive)
      return;

   int ret = runloop_iterate();

   task_queue_check();

   if (ret == -1)
   {
      main_exit(NULL);
      exit(0);
      return;
   }

   uint32_t runloop_flags = runloop_get_flags();
   if (!(runloop_flags & RUNLOOP_FLAG_IDLE))
      CFRunLoopWakeUp(CFRunLoopGetMain());
#endif
}
#endif

+ (CocoaView*)get
{
   CocoaView *view = (BRIDGE CocoaView*)nsview_get_ptr();
   if (!view)
   {
      view = [CocoaView new];
      nsview_set_ptr(view);
#if defined(IOS)
      view.displayLink = [CADisplayLink displayLinkWithTarget:view selector:@selector(step:)];
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000 || __TV_OS_VERSION_MAX_ALLOWED >= 150000
      if (@available(iOS 15.0, tvOS 15.0, *))
         [view.displayLink setPreferredFrameRateRange:CAFrameRateRangeDefault];
#endif
      [view.displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
#elif defined(OSX) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
      if (@available(macOS 14.0, *))
      {
         view.displayLink = [view displayLinkWithTarget:view selector:@selector(step:)];
         view.displayLink.preferredFrameRateRange = CAFrameRateRangeMake(60, 120, 120);
         [view.displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
      }
#endif
   }
   return view;
}

- (id)init
{
   self = [super init];

#if defined(OSX)
   [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   NSArray *array = [NSArray arrayWithObjects:NSColorPboardType, NSFilenamesPboardType, nil];
   [self registerForDraggedTypes:array];
#endif

#if defined(HAVE_COCOA)
   ui_window_cocoa_t cocoa_view;
   cocoa_view.data = (CocoaView*)self;
#endif

#if defined(OSX)
    video_driver_display_type_set(RARCH_DISPLAY_OSX);
    video_driver_display_set(0);
    video_driver_display_userdata_set((uintptr_t)self);
#endif

#if TARGET_OS_TV
   /* This causes all inputs to be handled by both mfi and uikit.
    *
    * For "extended gamepads" the only button we want to handle is 'cancel'
    * (buttonB), and only when the cancel button wouldn't do anything.
    */
   self.controllerUserInteractionEnabled = YES;
#endif
  
#if TARGET_OS_IOS
  self.shouldLockCurrentInterfaceOrientation = NO;
#endif

   return self;
}

#if TARGET_OS_TV
- (bool)menuIsAtTop
{
    struct menu_state *menu_st = menu_state_get_ptr();
    if (!(menu_st->flags & MENU_ST_FLAG_ALIVE)) /* content */
        return false;
    if (menu_st->flags & MENU_ST_FLAG_INP_DLG_KB_DISPLAY) /* search */
        return false;
    if (menu_st->selection_ptr != 0) /* not the first item */
        return false;
    if (menu_st->entries.list->menu_stack[0]->size != 1) /* submenu */
        return false;
    if (!string_is_equal(menu_st->entries.list->menu_stack[0]->list->label, /* not on the main menu */
                         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU)))
        return false;
    return true;
}

- (bool)isSiri:(GCController *)controller
{
    return (controller.microGamepad && !controller.extendedGamepad && [@"Remote" isEqualToString:controller.vendorName]);
}

- (bool)didMicroGamepadPress:(UIPressType)type
{
    /* Are these presses that controllers send? */
    if (@available(tvOS 14.3, *))
        if (type == UIPressTypePageUp || type == UIPressTypePageDown)
            return true;

    NSArray<GCController*>* controllers = [GCController controllers];

    bool foundSiri = false;
    bool nonSiriPress = false;
    for (GCController *controller in controllers) {
        if ([self isSiri:controller])
        {
            foundSiri = true;
            if (type == UIPressTypeSelect)
                return controller.microGamepad.buttonA.pressed;
            else if (type == UIPressTypePlayPause)
               return controller.microGamepad.buttonX.pressed;
        }
        else if (controller.extendedGamepad)
        {
            if (type == UIPressTypeUpArrow)
                nonSiriPress |= controller.extendedGamepad.dpad.up.pressed
                             || controller.extendedGamepad.leftThumbstick.up.pressed
                             || controller.extendedGamepad.rightThumbstick.up.pressed;
            else if (type == UIPressTypeDownArrow)
                nonSiriPress |= controller.extendedGamepad.dpad.down.pressed
                             || controller.extendedGamepad.leftThumbstick.down.pressed
                             || controller.extendedGamepad.rightThumbstick.down.pressed;
            else if (type == UIPressTypeLeftArrow)
                nonSiriPress |= controller.extendedGamepad.dpad.left.pressed
                             || controller.extendedGamepad.leftShoulder.pressed
                             || controller.extendedGamepad.leftTrigger.pressed
                             || controller.extendedGamepad.leftThumbstick.left.pressed
                             || controller.extendedGamepad.rightThumbstick.left.pressed;
            else if (type == UIPressTypeRightArrow)
                nonSiriPress |= controller.extendedGamepad.dpad.right.pressed
                             || controller.extendedGamepad.rightShoulder.pressed
                             || controller.extendedGamepad.rightTrigger.pressed
                             || controller.extendedGamepad.leftThumbstick.right.pressed
                            || controller.extendedGamepad.rightThumbstick.right.pressed;
            else if (type == UIPressTypeSelect)
                nonSiriPress |= controller.extendedGamepad.buttonA.pressed;
            else if (type == UIPressTypeMenu)
                nonSiriPress |= controller.extendedGamepad.buttonB.pressed;
            else if (type == UIPressTypePlayPause)
                nonSiriPress |= controller.extendedGamepad.buttonX.pressed;
        }
        else
        {
            /* we have a remote that is not extended. some of these remotes send
             * spurious presses. the only way to get them to work properly is to
             * make the siri remote work improperly. */
            nonSiriPress = true;
        }
    }

    if (!foundSiri || [controllers count] == 1)
        return foundSiri;

    return !nonSiriPress;
}

- (void)sendKeyForPress:(UIPressType)type down:(bool)down
{
    static NSDictionary<NSNumber *,NSArray<NSNumber*>*> *map;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithDictionary:@{
            @(UIPressTypeUpArrow):    @[ @(RETROK_UP),       @( 0 ) ],
            @(UIPressTypeDownArrow):  @[ @(RETROK_DOWN),     @( 0 ) ],
            @(UIPressTypeLeftArrow):  @[ @(RETROK_LEFT),     @( 0 ) ],
            @(UIPressTypeRightArrow): @[ @(RETROK_RIGHT),    @( 0 ) ],

            @(UIPressTypeSelect):     @[ @(RETROK_z),        @('z') ],
            @(UIPressTypeMenu)     :  @[ @(RETROK_x),        @('x') ],
            @(UIPressTypePlayPause):  @[ @(RETROK_s),        @('s') ],
        }];

        if (@available(tvOS 14.3, *))
        {
            [dict addEntriesFromDictionary:@{
                @(UIPressTypePageUp):     @[ @(RETROK_PAGEUP),   @( 0 ) ],
                @(UIPressTypePageDown):   @[ @(RETROK_PAGEDOWN), @( 0 ) ],
            }];
        }
        map = dict;
    });
    NSArray<NSNumber*>* keyvals = map[@(type)];
    if (!keyvals)
        return;
    apple_direct_input_keyboard_event(down, keyvals[0].intValue,
                                      keyvals[1].intValue, 0, RETRO_DEVICE_KEYBOARD);
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses
           withEvent:(UIPressesEvent *)event
{
    for (UIPress *press in presses)
    {
        bool has_key = false;
        if (@available(tvOS 14, *))
            has_key = !![press key];
        /* If we're at the top it doesn't matter who pressed it, we want to leave */
        if (press.type == UIPressTypeMenu && [self menuIsAtTop])
            [super pressesBegan:presses withEvent:event];
        else if (!has_key && [self didMicroGamepadPress:press.type])
            [self sendKeyForPress:press.type down:true];
        else if (has_key)
            [super pressesBegan:[NSSet setWithObject:press] withEvent:event];
    }
}

-(void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
    for (UIPress *press in presses) {
        if (press.type == UIPressTypeSelect || press.type == UIPressTypePlayPause)
            [self sendKeyForPress:press.type down:false];
        else
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
                [[CocoaView get] sendKeyForPress:press.type down:false];
            });
    }
}

-(void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
}

-(void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
}

-(void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
}

-(void)touchesEstimatedPropertiesUpdated:(NSSet<UITouch *> *)touches
{
}

-(void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
}

-(void)handleSiriSwipe:(id)sender
{
   UISwipeGestureRecognizer *gestureRecognizer = (UISwipeGestureRecognizer*)sender;
   unsigned code;
   switch (gestureRecognizer.direction)
   {
      case UISwipeGestureRecognizerDirectionUp:    code = RETROK_UP;    break;
      case UISwipeGestureRecognizerDirectionDown:  code = RETROK_DOWN;  break;
      case UISwipeGestureRecognizerDirectionLeft:  code = RETROK_LEFT;  break;
      case UISwipeGestureRecognizerDirectionRight: code = RETROK_RIGHT; break;
   }
   apple_direct_input_keyboard_event(true,  code, 0, 0, RETRO_DEVICE_KEYBOARD);
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
      apple_direct_input_keyboard_event(false, code, 0, 0, RETRO_DEVICE_KEYBOARD);
   });
}
#endif

#if TARGET_OS_IOS

#pragma mark UIDocumentPickerViewController

-(void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentAtURL:(NSURL *)url
{
   NSFileManager *manager = [NSFileManager defaultManager];
   NSString     *filename = (NSString*)url.path.lastPathComponent;
   NSError         *error = nil;
   settings_t *settings   = config_get_ptr();
   char fullpath[PATH_MAX_LENGTH] = {0};
   fill_pathname_join_special(fullpath, settings->paths.directory_core_assets, [filename UTF8String], sizeof(fullpath));
   NSString  *destination = [NSString stringWithUTF8String:fullpath];
   NSString *documentsDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
   /* Copy file to documents directory if it's not already
    * inside Documents directory */
   if (![[url path] containsString:documentsDir])
      if (![manager fileExistsAtPath:destination])
         [manager copyItemAtPath:[url path] toPath:destination error:&error];
   if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
      action_scan_file(fullpath, NULL, 0, 0);
   else
   {
      cocoa_file_load_with_detect_core(fullpath);
   }
}

-(void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
}

-(void)showDocumentPicker
{
   UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc]
                                                     initWithDocumentTypes:@[(NSString *)kUTTypeDirectory,
                                                                             (NSString *)kUTTypeItem]
                                                     inMode:UIDocumentPickerModeImport];
   documentPicker.delegate = self;
   documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;
   [self presentViewController:documentPicker animated:YES completion:nil];
}

#endif

#if defined(OSX)
- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];
/* forward declarations */
#if defined(HAVE_OPENGL)
   void cocoa_gl_gfx_ctx_update(void);
   cocoa_gl_gfx_ctx_update();
#endif
}

/* Stop the annoying sound when pressing a key. */
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)keyDown:(NSEvent*)theEvent { }

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];
    NSPasteboard           *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSFilenamesPboardType] )
    {
        if (sourceDragMask & NSDragOperationCopy)
            return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
#if 0
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSURLPboardType])
    {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        NSString    *s = [fileURL path];
    }
#endif
    return YES;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender { [self setNeedsDisplay: YES]; }

#elif TARGET_OS_IOS
-(void) showNativeMenu
{
    dispatch_async(dispatch_get_main_queue(), ^{
        command_event(CMD_EVENT_MENU_TOGGLE, NULL);
    });
}

#ifdef HAVE_IOS_CUSTOMKEYBOARD
-(void)toggleCustomKeyboardUsingSwipe:(id)sender {
    UISwipeGestureRecognizer *gestureRecognizer = (UISwipeGestureRecognizer*)sender;
    [self.keyboardController.view setHidden:gestureRecognizer.direction == UISwipeGestureRecognizerDirectionDown];
    [self updateOverlayAndFocus];
}

-(void)toggleCustomKeyboard {
    [self.keyboardController.view setHidden:!self.keyboardController.view.isHidden];
    [self updateOverlayAndFocus];
}
#endif

-(void) updateOverlayAndFocus
{
#ifdef HAVE_IOS_CUSTOMKEYBOARD
    int cmdData = self.keyboardController.view.isHidden ? 0 : 1;
    command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &cmdData);
    if (self.keyboardController.view.isHidden)
        command_event(CMD_EVENT_OVERLAY_INIT, NULL);
    else
        command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
#endif
}

-(BOOL)prefersHomeIndicatorAutoHidden { return YES; }
-(void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    if (@available(iOS 11, *))
    {
        [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
            [self adjustViewFrameForSafeArea];
        } completion:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        }];
    }
}

-(void)adjustViewFrameForSafeArea
{
   /* This is for adjusting the view frame to account for
    * the notch in iPhone X phones */
   if (@available(iOS 11, *))
   {
      settings_t *settings               = config_get_ptr();
      RAScreen *screen                   = (BRIDGE RAScreen*)cocoa_screen_get_chosen();
      CGRect screenSize                  = [screen bounds];
      UIEdgeInsets inset                 = [[UIApplication sharedApplication] delegate].window.safeAreaInsets;
      UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];

      if (settings->bools.video_notch_write_over_enable)
      {
         self.view.frame = CGRectMake(screenSize.origin.x,
                     screenSize.origin.y,
                     screenSize.size.width,
                     screenSize.size.height);
         return;
      }

      switch (orientation)
      {
         case UIInterfaceOrientationPortrait:
            self.view.frame = CGRectMake(screenSize.origin.x,
                  screenSize.origin.y + inset.top,
                  screenSize.size.width,
                  screenSize.size.height - inset.top);
            break;
         case UIInterfaceOrientationLandscapeLeft:
            self.view.frame = CGRectMake(screenSize.origin.x + inset.right,
                  screenSize.origin.y,
                  screenSize.size.width - inset.right * 2,
                  screenSize.size.height);
            break;
         case UIInterfaceOrientationLandscapeRight:
            self.view.frame = CGRectMake(screenSize.origin.x + inset.left,
                  screenSize.origin.y,
                  screenSize.size.width - inset.left * 2,
                  screenSize.size.height);
            break;
         default:
            self.view.frame = screenSize;
            break;
      }
   }
}

- (void)viewWillLayoutSubviews
{
   [self adjustViewFrameForSafeArea];
#ifdef HAVE_IOS_CUSTOMKEYBOARD
   [self.view bringSubviewToFront:self.keyboardController.view];
#endif
#if HAVE_IOS_SWIFT
    [self.view bringSubviewToFront:self.helperBarView];
#endif
}

/* NOTE: This version runs on iOS6+. */
- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
  if (@available(iOS 16, *)) {
    if (self.shouldLockCurrentInterfaceOrientation) {
      return 1 << self.lockInterfaceOrientation;
    } else {
      return (UIInterfaceOrientationMask)apple_frontend_settings.orientation_flags;
    }
  } else {
    return (UIInterfaceOrientationMask)apple_frontend_settings.orientation_flags;
  }
}

/* NOTE: This does not run on iOS 16+ */
-(BOOL)shouldAutorotate {
  if (self.shouldLockCurrentInterfaceOrientation) {
    return NO;
  }
  return YES;
}

/* NOTE: This version runs on iOS2-iOS5, but not iOS6+. */
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
   unsigned orientation_flags = apple_frontend_settings.orientation_flags;

   switch (interfaceOrientation)
   {
      case UIInterfaceOrientationPortrait:
         return (orientation_flags
               & UIInterfaceOrientationMaskPortrait);
      case UIInterfaceOrientationPortraitUpsideDown:
         return (orientation_flags
               & UIInterfaceOrientationMaskPortraitUpsideDown);
      case UIInterfaceOrientationLandscapeLeft:
         return (orientation_flags
               & UIInterfaceOrientationMaskLandscapeLeft);
      case UIInterfaceOrientationLandscapeRight:
         return (orientation_flags
               & UIInterfaceOrientationMaskLandscapeRight);

      default:
         break;
   }

   return (orientation_flags
            & UIInterfaceOrientationMaskAll);
}
#endif

#ifdef HAVE_COCOATOUCH

-(BOOL) prefersPointerLocked API_AVAILABLE(ios(14.0))
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
   if (!apple)
      return NO;
   return apple->mouse_grabbed;
}

#pragma mark - UIViewController Lifecycle

-(void)loadView {
#if defined(HAVE_COCOA_METAL)
   self.view       = [UIView new];
#else
   self.view       = (BRIDGE GLKView*)glkitview_init();
#endif
}

-(void)viewDidLoad {
    [super viewDidLoad];
#if TARGET_OS_IOS
    UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(showNativeMenu)];
    swipe.numberOfTouchesRequired   = 4;
    swipe.delegate                  = self;
    swipe.direction                 = UISwipeGestureRecognizerDirectionDown;
    [self.view addGestureRecognizer:swipe];
#ifdef HAVE_IOS_TOUCHMOUSE
    if (@available(iOS 13, *))
        [self setupMouseSupport];
#endif
#ifdef HAVE_IOS_CUSTOMKEYBOARD
    if (@available(iOS 13, *))
        [self setupEmulatorKeyboard];
    UISwipeGestureRecognizer *showKeyboardSwipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(toggleCustomKeyboardUsingSwipe:)];
    showKeyboardSwipe.numberOfTouchesRequired   = 3;
    showKeyboardSwipe.direction                 = UISwipeGestureRecognizerDirectionUp;
    showKeyboardSwipe.delegate                  = self;
    [self.view addGestureRecognizer:showKeyboardSwipe];
    UISwipeGestureRecognizer *hideKeyboardSwipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(toggleCustomKeyboardUsingSwipe:)];
    hideKeyboardSwipe.numberOfTouchesRequired   = 3;
    hideKeyboardSwipe.direction                 = UISwipeGestureRecognizerDirectionDown;
    hideKeyboardSwipe.delegate                  = self;
    [self.view addGestureRecognizer:hideKeyboardSwipe];
#endif
#if defined(HAVE_IOS_TOUCHMOUSE) || defined(HAVE_IOS_CUSTOMKEYBOARDS)
    if (@available(iOS 13, *))
        [self setupHelperBar];
#endif
#elif TARGET_OS_TV
    UISwipeGestureRecognizer *siriSwipeUp    = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(handleSiriSwipe:)];
    siriSwipeUp.direction                    = UISwipeGestureRecognizerDirectionUp;
    siriSwipeUp.delegate                     = self;
    [self.view addGestureRecognizer:siriSwipeUp];
    UISwipeGestureRecognizer *siriSwipeDown  = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(handleSiriSwipe:)];
    siriSwipeDown.direction                  = UISwipeGestureRecognizerDirectionDown;
    siriSwipeDown.delegate                   = self;
    [self.view addGestureRecognizer:siriSwipeDown];
    UISwipeGestureRecognizer *siriSwipeLeft  = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(handleSiriSwipe:)];
    siriSwipeLeft.direction                  = UISwipeGestureRecognizerDirectionLeft;
    siriSwipeLeft.delegate                   = self;
    [self.view addGestureRecognizer:siriSwipeLeft];
    UISwipeGestureRecognizer *siriSwipeRight = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(handleSiriSwipe:)];
    siriSwipeRight.direction                 = UISwipeGestureRecognizerDirectionRight;
    siriSwipeRight.delegate                  = self;
    [self.view addGestureRecognizer:siriSwipeRight];
#endif
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer {
    return YES;
}


- (void)viewDidAppear:(BOOL)animated
{
#if TARGET_OS_IOS
    if (@available(iOS 11.0, *))
        [self setNeedsUpdateOfHomeIndicatorAutoHidden];
#endif
}

-(void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
#if TARGET_OS_TV
    [[WebServer sharedInstance] startUploader];
    [WebServer sharedInstance].webUploader.delegate = self;
#endif
}

#if TARGET_OS_IOS && HAVE_IOS_TOUCHMOUSE

#pragma mark EmulatorTouchMouseHandlerDelegate

-(void)handleMouseClickWithIsLeftClick:(BOOL)isLeftClick isPressed:(BOOL)isPressed
{
    cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
    if (!apple)
        return;
    NSUInteger buttonIndex = isLeftClick ? 0 : 1;
    if (isPressed)
        apple->mouse_buttons |= (1 << buttonIndex);
    else
        apple->mouse_buttons &= ~(1 << buttonIndex);
}

-(void)handleMouseMoveWithX:(CGFloat)x y:(CGFloat)y
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*) input_state_get_ptr()->current_data;
   if (!apple)
      return;
   apple->mouse_rel_x = (int16_t)x;
   apple->mouse_rel_y = (int16_t)y;
   /* use location position to track pointer */
   if (@available(iOS 13.4, *))
   {
      apple->window_pos_x = 0;
      apple->window_pos_y = 0;
   }
}

#endif

#pragma mark GCDWebServerDelegate
- (void)webServerDidCompleteBonjourRegistration:(GCDWebServer*)server
{
    NSMutableString *servers = [[NSMutableString alloc] init];
    if (server.serverURL != nil)
        [servers appendString:[NSString stringWithFormat:@"%@",server.serverURL]];
    if (servers.length > 0)
        [servers appendString:@"\n\n"];
    if (server.bonjourServerURL != nil)
        [servers appendString:[NSString stringWithFormat:@"%@",server.bonjourServerURL]];

#if TARGET_OS_TV || TARGET_OS_IOS
    settings_t *settings = config_get_ptr();
    if (!settings->bools.gcdwebserver_alert)
        return;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Welcome to RetroArch" message:[NSString stringWithFormat:@"To transfer files from your computer, go to one of these addresses on your web browser:\n\n%@",servers] preferredStyle:UIAlertControllerStyleAlert];
#if TARGET_OS_TV
        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
            style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
                rarch_start_draw_observer();
        }]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Don't Show Again"
            style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
                rarch_start_draw_observer();
                configuration_set_bool(settings, settings->bools.gcdwebserver_alert, false);
        }]];
#elif TARGET_OS_IOS
        [alert addAction:[UIAlertAction actionWithTitle:@"Stop Server" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
            [[WebServer sharedInstance] webUploader].delegate = nil;
            [[WebServer sharedInstance] stopUploader];
        }]];
#endif
        [self presentViewController:alert animated:YES completion:^{
            rarch_stop_draw_observer();
        }];
    });
#endif
}

#endif

@end

#if TARGET_OS_IOS
void ios_show_file_sheet(void)
{
   [[CocoaView get] showDocumentPicker];
}
#endif

void *cocoa_screen_get_chosen(void)
{
    unsigned monitor_index;
    settings_t *settings = config_get_ptr();
    NSArray *screens     = [RAScreen screens];
    if (!screens || !settings)
        return NULL;

    monitor_index        = settings->uints.video_monitor_index;

    if (monitor_index >= screens.count)
        return (BRIDGE void*)screens;
    return ((BRIDGE void*)[screens objectAtIndex:monitor_index]);
}

bool cocoa_has_focus(void *data)
{
#if defined(HAVE_COCOATOUCH)
    /* if we are running, we are foregrounded */
    return true;
#else
    return [NSApp isActive];
#endif
}

void cocoa_show_mouse(void *data, bool state)
{
#ifdef OSX
    if (state)
        [NSCursor unhide];
    else
        [NSCursor hide];
#endif
}

#ifdef OSX
#if MAC_OS_X_VERSION_10_7
/* NOTE: backingScaleFactor only available on MacOS X 10.7 and up. */
float cocoa_screen_get_backing_scale_factor(void)
{
    static float
    backing_scale_def        = 0.0f;
    if (backing_scale_def == 0.0f)
    {
        RAScreen *screen      = (BRIDGE RAScreen*)cocoa_screen_get_chosen();
        if (!screen)
            return 1.0f;
        backing_scale_def     = [screen backingScaleFactor];
    }
    return backing_scale_def;
}
#else
float cocoa_screen_get_backing_scale_factor(void) { return 1.0f; }
#endif
#else
static float get_from_selector(
      Class obj_class, id obj_id, SEL selector, CGFloat *ret)
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
                                [obj_class instanceMethodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    [invocation setTarget:obj_id];
    [invocation invoke];
    [invocation getReturnValue:ret];
    RELEASE(invocation);
    return *ret;
}

/* NOTE: nativeScale only available on iOS 8.0 and up. */
float cocoa_screen_get_native_scale(void)
{
    SEL selector;
    static CGFloat ret   = 0.0f;
    RAScreen *screen     = NULL;

    if (ret != 0.0f)
        return ret;
    if (!(screen = (BRIDGE RAScreen*)cocoa_screen_get_chosen()))
        return 0.0f;

    selector            = NSSelectorFromString(BOXSTRING("nativeScale"));

    if ([screen respondsToSelector:selector])
        ret                 = (float)get_from_selector(
              [screen class], screen, selector, &ret);
    else
    {
        ret                 = 1.0f;
        selector            = NSSelectorFromString(BOXSTRING("scale"));
        if ([screen respondsToSelector:selector])
            ret              = screen.scale;
    }

#if TARGET_OS_TV
    if (ret < 1.0f)
       ret = 1.0f;
#endif
    return ret;
}
#endif

void *nsview_get_ptr(void)
{
#if defined(OSX)
    video_driver_display_type_set(RARCH_DISPLAY_OSX);
    video_driver_display_set(0);
    video_driver_display_userdata_set((uintptr_t)g_instance);
#endif
    return (BRIDGE void *)g_instance;
}

void nsview_set_ptr(CocoaView *p) { g_instance = p; }

CocoaView *cocoaview_get(void)
{
#if defined(HAVE_COCOA_METAL)
    return (CocoaView*)apple_platform.renderView;
#elif defined(HAVE_COCOA)
    return g_instance;
#else
    /* TODO/FIXME - implement */
    return NULL;
#endif
}

#ifdef OSX
bool cocoa_get_metrics(
      void *data, enum display_metric_types type,
      float *value)
{
   RAScreen *screen              = (BRIDGE RAScreen*)cocoa_screen_get_chosen();
   NSDictionary *desc            = [screen deviceDescription];
   CGSize  display_physical_size = CGDisplayScreenSize(
         [[desc objectForKey:@"NSScreenNumber"] unsignedIntValue]);

   float   physical_width        = display_physical_size.width;
   float   physical_height       = display_physical_size.height;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         {
            NSSize disp_pixel_size = [[desc objectForKey:NSDeviceSize] sizeValue];
            float dispwidth = disp_pixel_size.width;
            float   scale   = cocoa_screen_get_backing_scale_factor();
            float   dpi     = (dispwidth / physical_width) * 25.4f * scale;
            *value          = dpi;
         }
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
}
#else
bool cocoa_get_metrics(
      void *data, enum display_metric_types type,
      float *value)
{
   RAScreen *screen              = (BRIDGE RAScreen*)cocoa_screen_get_chosen();
   float   scale                 = cocoa_screen_get_native_scale();
   CGRect  screen_rect           = [screen bounds];
   float   physical_width        = screen_rect.size.width  * scale;
   float   physical_height       = screen_rect.size.height * scale;
   float   dpi                   = 160                     * scale;
   NSInteger idiom_type          = UI_USER_INTERFACE_IDIOM();

   switch (idiom_type)
   {
      case -1: /* UIUserInterfaceIdiomUnspecified */
         /* TODO */
         break;
      case UIUserInterfaceIdiomPad:
         dpi = 132 * scale;
         break;
      case UIUserInterfaceIdiomPhone:
         {
            CGFloat maxSize = fmaxf(physical_width, physical_height);
            /* Larger iPhones: iPhone Plus, X, XR, XS, XS Max, 11, 11 Pro Max */
            if (maxSize >= 2208.0)
               dpi = 81 * scale;
            else
               dpi = 163 * scale;
         }
         break;
      case UIUserInterfaceIdiomTV:
      case UIUserInterfaceIdiomCarPlay:
         /* TODO */
         break;
   }

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         *value = dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
}
#endif

config_file_t *open_userdefaults_config_file(void)
{
   config_file_t *conf = NULL;
   NSString *backup = [NSUserDefaults.standardUserDefaults stringForKey:@FILE_PATH_MAIN_CONFIG];
   if ([backup length] > 0)
   {
      char *str = strdup(backup.UTF8String);
      conf = config_file_new_from_string(str, path_get(RARCH_PATH_CONFIG));
      free(str);
      /* If we are falling back to the NSUserDefaults backup of the config file,
       * it's likely because the OS has deleted all of our cache, including our
       * extracted assets. This will cause re-extraction */
      config_set_int(conf, "bundle_assets_extract_last_version", 0);
   }
   return conf;
}

void write_userdefaults_config_file(void)
{
   NSString *conf = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path_get(RARCH_PATH_CONFIG)]
                                              encoding:NSUTF8StringEncoding
                                                 error:nil];
   if (conf)
      [NSUserDefaults.standardUserDefaults setObject:conf forKey:@FILE_PATH_MAIN_CONFIG];
}

#if TARGET_OS_TV
static NSDictionary *topshelfDictForEntry(const struct playlist_entry *entry, gfx_thumbnail_path_data_t *path_data)
{
   NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithDictionary:@{
      @"id": [NSString stringWithUTF8String:entry->path],
      @"title": [NSString stringWithUTF8String:
                             (string_is_empty(entry->label) ? path_basename(entry->path) : entry->label)],
   }];
   if (!string_is_empty(path_data->content_db_name))
   {
      const char *img_name = NULL;
      if (gfx_thumbnail_get_img_name(path_data, &img_name, PLAYLIST_THUMBNAIL_FLAG_STD_NAME))
         dict[@"img"] = [NSString stringWithFormat:@"https://thumbnails.libretro.com/%s/Named_Boxarts/%s",
                         path_data->content_db_name, img_name];
   }
   NSURLComponents *play = [[NSURLComponents alloc] initWithString:@"retroarch://topshelf"];
   [play setQueryItems:@[
      [[NSURLQueryItem alloc] initWithName:@"path" value:[NSString stringWithUTF8String:entry->path]],
      [[NSURLQueryItem alloc] initWithName:@"core_path" value:[NSString stringWithUTF8String:entry->core_path]],
   ]];
   dict[@"play"] = [play string];
   return dict;
}

void update_topshelf(void)
{
   if (@available(tvOS 13.0, *))
   {
      NSUserDefaults *ud = [[NSUserDefaults alloc] initWithSuiteName:kRetroArchAppGroup];
      if (!ud)
         return;

      NSMutableDictionary *contentDict = [NSMutableDictionary dictionaryWithCapacity:2];
      const struct playlist_entry *entry;
      gfx_thumbnail_path_data_t *thumbnail_path_data = gfx_thumbnail_path_init();

      settings_t *settings     = config_get_ptr();
      bool history_list_enable = settings->bools.history_list_enable;
      if (history_list_enable && playlist_size(g_defaults.content_history) > 0)
      {
         NSMutableArray *array = [NSMutableArray arrayWithCapacity:playlist_size(g_defaults.content_history)];
         NSString *key = [NSString stringWithUTF8String:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB)];
         for (size_t i = 0; i < 5 && i < playlist_size(g_defaults.content_history); i++)
         {
            gfx_thumbnail_path_reset(thumbnail_path_data);
            gfx_thumbnail_set_content_playlist(thumbnail_path_data, g_defaults.content_history, i);
            playlist_get_index(g_defaults.content_history, i, &entry);
            [array addObject:topshelfDictForEntry(entry, thumbnail_path_data)];
         }
         contentDict[key] = array;
      }

      if (playlist_size(g_defaults.content_favorites) > 0)
      {
         NSMutableArray *array = [NSMutableArray arrayWithCapacity:playlist_size(g_defaults.content_favorites)];
         NSString *key = [NSString stringWithUTF8String:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB)];
         for (size_t i = 0; i < 5 && i < playlist_size(g_defaults.content_favorites); i++)
         {
            gfx_thumbnail_path_reset(thumbnail_path_data);
            gfx_thumbnail_set_content_playlist(thumbnail_path_data, g_defaults.content_favorites, i);
            playlist_get_index(g_defaults.content_favorites, i, &entry);
            [array addObject:topshelfDictForEntry(entry, thumbnail_path_data)];
         }
         contentDict[key] = array;
      }

      [ud setObject:contentDict forKey:@"topshelf"];
      [TVTopShelfContentProvider topShelfContentDidChange];
   }
}
#endif

void cocoa_file_load_with_detect_core(const char *filename)
{
   /* largely copied from file_load_with_detect_core() in menu_cbs_ok.c */
   core_info_list_t *list = NULL;
   const core_info_t *info = NULL;
   size_t supported = 0;

   if (path_is_compressed_file(filename))
   {
      generic_action_ok_displaylist_push(filename, NULL,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
            FILE_TYPE_CARCHIVE, 0, 0, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE);
      return;
   }

   core_info_get_list(&list);
   core_info_list_get_supported_cores(list, filename, &info, &supported);
   if (supported > 1)
   {
      struct menu_state *menu_st          = menu_state_get_ptr();
      menu_handle_t *menu                 = menu_st->driver_data;
      strlcpy(menu->deferred_path, filename, sizeof(menu->deferred_path));
      strlcpy(menu->detect_content_path, filename, sizeof(menu->detect_content_path));
      generic_action_ok_displaylist_push(filename, NULL, NULL, FILE_TYPE_NONE, 0, 0, ACTION_OK_DL_DEFERRED_CORE_LIST);
   }
   else if (supported == 1)
   {
      content_ctx_info_t content_info;

      content_info.argc        = 0;
      content_info.argv        = NULL;
      content_info.args        = NULL;
      content_info.environ_get = NULL;

      task_push_load_content_with_new_core_from_menu(
               info->path, filename,
               &content_info,
               CORE_TYPE_PLAIN,
               NULL, NULL);
   }
}

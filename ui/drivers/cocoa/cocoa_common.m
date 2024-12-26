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

#ifdef HAVE_COCOATOUCH
#import "../../../pkg/apple/WebServer/GCDWebUploader/GCDWebUploader.h"
#import "WebServer.h"
#ifdef HAVE_IOS_SWIFT
#import "RetroArch-Swift.h"
#endif
#endif

#include "../../../configuration.h"
#include "../../../paths.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"

#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)
id<ApplePlatform> apple_platform;
#else
id apple_platform;
#endif

static CocoaView* g_instance;

#ifdef HAVE_COCOATOUCH
void *glkitview_init(void);

@interface CocoaView()<GCDWebUploaderDelegate, UIGestureRecognizerDelegate
#ifdef HAVE_IOS_TOUCHMOUSE
,EmulatorTouchMouseHandlerDelegate
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

+ (CocoaView*)get
{
   CocoaView *view = (BRIDGE CocoaView*)nsview_get_ptr();
   if (!view)
   {
      view = [CocoaView new];
      nsview_set_ptr(view);
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

- (bool)didMicroGamepadPress:(UIPressType)type
{
    NSArray<GCController*>* controllers = [GCController controllers];
    if ([controllers count] == 1)
        return !controllers[0].extendedGamepad;

    /* Are these presses that controllers send? */
    if (@available(tvOS 14.3, *))
        if (type == UIPressTypePageUp || type == UIPressTypePageDown)
            return true;

    bool microPress = false;
    bool extendedPress = false;
    for (GCController *controller in [GCController controllers]) {
        if (controller.extendedGamepad)
        {
            if (type == UIPressTypeUpArrow)
                extendedPress |= controller.extendedGamepad.dpad.up.pressed
                              || controller.extendedGamepad.leftThumbstick.up.pressed
                              || controller.extendedGamepad.rightThumbstick.up.pressed;
            else if (type == UIPressTypeDownArrow)
                extendedPress |= controller.extendedGamepad.dpad.down.pressed
                              || controller.extendedGamepad.leftThumbstick.down.pressed
                              || controller.extendedGamepad.rightThumbstick.down.pressed;
            else if (type == UIPressTypeLeftArrow)
                extendedPress |= controller.extendedGamepad.dpad.left.pressed
                              || controller.extendedGamepad.leftShoulder.pressed
                              || controller.extendedGamepad.leftTrigger.pressed
                              || controller.extendedGamepad.leftThumbstick.left.pressed
                              || controller.extendedGamepad.rightThumbstick.left.pressed;
            else if (type == UIPressTypeRightArrow)
                extendedPress |= controller.extendedGamepad.dpad.right.pressed
                              || controller.extendedGamepad.rightShoulder.pressed
                              || controller.extendedGamepad.rightTrigger.pressed
                              || controller.extendedGamepad.leftThumbstick.right.pressed
                              || controller.extendedGamepad.rightThumbstick.right.pressed;
            else if (type == UIPressTypeSelect)
                extendedPress |= controller.extendedGamepad.buttonA.pressed;
            else if (type == UIPressTypeMenu)
                extendedPress |= controller.extendedGamepad.buttonB.pressed;
            else if (type == UIPressTypePlayPause)
                extendedPress |= controller.extendedGamepad.buttonX.pressed;

        }
        else if (controller.microGamepad)
        {
            if (type == UIPressTypeSelect)
                microPress |= controller.microGamepad.buttonA.pressed;
            else if (type == UIPressTypePlayPause)
                microPress |= controller.microGamepad.buttonX.pressed;
            else if (@available(tvOS 13, *)) {
                if (type == UIPressTypeMenu)
                    extendedPress |= controller.microGamepad.buttonMenu.pressed ||
                    controller.microGamepad.buttonMenu.isPressed;
            }
        }
    }

    return microPress || !extendedPress;
}

- (void)sendKeyForPress:(UIPressType)type down:(bool)down
{
    static NSDictionary<NSNumber *,NSArray<NSNumber*>*> *map;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        map = @{
            @(UIPressTypeUpArrow):    @[ @(RETROK_UP),       @( 0 ) ],
            @(UIPressTypeDownArrow):  @[ @(RETROK_DOWN),     @( 0 ) ],
            @(UIPressTypeLeftArrow):  @[ @(RETROK_LEFT),     @( 0 ) ],
            @(UIPressTypeRightArrow): @[ @(RETROK_RIGHT),    @( 0 ) ],

            @(UIPressTypeSelect):     @[ @(RETROK_z),        @('z') ],
            @(UIPressTypeMenu)     :  @[ @(RETROK_x),        @('x') ],
            @(UIPressTypePlayPause):  @[ @(RETROK_s),        @('s') ],

            @(UIPressTypePageUp):     @[ @(RETROK_PAGEUP),   @( 0 ) ],
            @(UIPressTypePageDown):   @[ @(RETROK_PAGEDOWN), @( 0 ) ],
        };
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
        /* If we're at the top it doesn't matter who pressed it, we want to leave */
        if (press.type == UIPressTypeMenu && [self menuIsAtTop])
            [super pressesBegan:presses withEvent:event];
        else if ([self didMicroGamepadPress:press.type])
            [self sendKeyForPress:press.type down:true];
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
      RAScreen *screen                   = (BRIDGE RAScreen*)cocoa_screen_get_chosen();
      CGRect screenSize                  = [screen bounds];
      UIEdgeInsets inset                 = [[UIApplication sharedApplication] delegate].window.safeAreaInsets;
      UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
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
    [self setupMouseSupport];
#endif
#ifdef HAVE_IOS_CUSTOMKEYBOARD
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
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
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

-(void)handlePointerMoveWithX:(CGFloat)x y:(CGFloat)y
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)
      input_state_get_ptr()->current_data;
   if (!apple)
      return;
   apple->window_pos_x = (int16_t)x;
   apple->window_pos_y = (int16_t)y;
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
    return ([[UIApplication sharedApplication] applicationState]
            == UIApplicationStateActive);
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

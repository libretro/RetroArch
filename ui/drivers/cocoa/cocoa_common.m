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
#include "../ui_cocoa.h"

#include <retro_assert.h>

#include "../../../verbosity.h"

#include "../../../input/drivers/cocoa_input.h"
#include "../../../retroarch.h"

#ifdef HAVE_COCOATOUCH
#import "GCDWebUploader.h"
#import "WebServer.h"
#endif



/* forward declarations */
void cocoagl_gfx_ctx_update(void);
void *glkitview_init(void);

#ifdef HAVE_COCOATOUCH
@interface CocoaView()<GCDWebUploaderDelegate> {

}
@end
#endif

@implementation CocoaView

#if defined(HAVE_COCOA_METAL)
- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window {
   return YES;
}
#endif

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
- (void)scrollWheel:(NSEvent *)theEvent {
    cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();
    (void)apple;
}
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

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
   [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#endif

#if defined(HAVE_COCOA)
   ui_window_cocoa_t cocoa_view;
   cocoa_view.data = (CocoaView*)self;

   [self registerForDraggedTypes:[NSArray arrayWithObjects:NSColorPboardType, NSFilenamesPboardType, nil]];
#elif defined(HAVE_COCOA_METAL)
   [self registerForDraggedTypes:@[NSColorPboardType, NSFilenamesPboardType]];
#elif defined(HAVE_COCOATOUCH)
   self.view = (BRIDGE GLKView*)glkitview_init();
#if TARGET_OS_IOS
    UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(showNativeMenu)];
    swipe.numberOfTouchesRequired = 4;
    swipe.direction = UISwipeGestureRecognizerDirectionDown;
    [self.view addGestureRecognizer:swipe];
#endif
#endif
    
#if defined(HAVE_COCOA)
    video_driver_display_type_set(RARCH_DISPLAY_OSX);
    video_driver_display_set(0);
    video_driver_display_userdata_set((uintptr_t)self);
#elif defined(HAVE_COCOA_METAL)
    video_driver_display_type_set(RARCH_DISPLAY_OSX);
    video_driver_display_set(0);
    video_driver_display_userdata_set((uintptr_t)self);
#endif

   return self;
}

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];

   cocoagl_gfx_ctx_update();
}

/* Stop the annoying sound when pressing a key. */
- (BOOL)acceptsFirstResponder
{
   return YES;
}

- (BOOL)isFlipped
{
   return YES;
}

- (void)keyDown:(NSEvent*)theEvent
{
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSFilenamesPboardType] )
    {
        if (sourceDragMask & NSDragOperationCopy)
            return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSURLPboardType])
    {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        NSString *s = [fileURL path];
        if (s != nil)
        {
           RARCH_LOG("Drop name is: %s\n", [s UTF8String]);
        }
    }
    return YES;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay: YES];
}

#elif TARGET_OS_IOS
-(void) showNativeMenu {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[RetroArch_iOS get] toggleUI];
    });
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    return UIRectEdgeBottom;
}

-(BOOL)prefersHomeIndicatorAutoHidden
{
    return NO;
}

-(void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    if (@available(iOS 11, *)) {
        [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
            [self adjustViewFrameForSafeArea];
        } completion:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        }];
    }
}

-(void)adjustViewFrameForSafeArea {
    // This is for adjusting the view frame to account for the notch in iPhone X phones
    if (@available(iOS 11, *)) {
        RAScreen *screen  = (BRIDGE RAScreen*)get_chosen_screen();
        CGRect screenSize = [screen bounds];
        UIEdgeInsets inset = [[UIApplication sharedApplication] delegate].window.safeAreaInsets;
        UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
        CGRect newFrame = screenSize;
        if ( orientation == UIInterfaceOrientationPortrait ) {
            newFrame = CGRectMake(screenSize.origin.x, screenSize.origin.y + inset.top, screenSize.size.width, screenSize.size.height - inset.top);
		} else if ( orientation == UIInterfaceOrientationLandscapeLeft ) {
		    newFrame = CGRectMake(screenSize.origin.x + inset.right, screenSize.origin.y, screenSize.size.width - inset.right * 2, screenSize.size.height);
		} else if ( orientation == UIInterfaceOrientationLandscapeRight ) {
		    newFrame = CGRectMake(screenSize.origin.x + inset.left, screenSize.origin.y, screenSize.size.width - inset.left * 2, screenSize.size.height);
        }
        self.view.frame = newFrame;
    }
}

- (void)viewWillLayoutSubviews
{
   float width = 0.0f, height = 0.0f, tenpctw, tenpcth;
   RAScreen *screen  = (BRIDGE RAScreen*)get_chosen_screen();
   UIInterfaceOrientation orientation = self.interfaceOrientation;
   CGRect screenSize = [screen bounds];
   SEL selector = NSSelectorFromString(BOXSTRING("coordinateSpace"));

    if ([screen respondsToSelector:selector])
    {
        screenSize  = [[screen coordinateSpace] bounds];
        width       = CGRectGetWidth(screenSize);
        height      = CGRectGetHeight(screenSize);
    }
    else
    {
        width       = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
        height      = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);
    }

   tenpctw          = width  / 10.0f;
   tenpcth          = height / 10.0f;

   [self adjustViewFrameForSafeArea];
}

/* NOTE: This version runs on iOS6+. */
- (NSUInteger)supportedInterfaceOrientations
{
   return (NSUInteger)apple_frontend_settings.orientation_flags;
}

/* NOTE: This version runs on iOS2-iOS5, but not iOS6+. */
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
   switch (interfaceOrientation)
   {
      case UIInterfaceOrientationPortrait:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortrait);
      case UIInterfaceOrientationPortraitUpsideDown:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortraitUpsideDown);
      case UIInterfaceOrientationLandscapeLeft:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeLeft);
      case UIInterfaceOrientationLandscapeRight:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeRight);

      default:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskAll);
   }

   return YES;
}
#endif

#ifdef HAVE_COCOATOUCH
- (void)viewDidAppear:(BOOL)animated
{
#if TARGET_OS_IOS
    if (@available(iOS 11.0, *)) {
        [self setNeedsUpdateOfHomeIndicatorAutoHidden];
    }
#elif TARGET_OS_TV

#endif
}

-(void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
#if TARGET_OS_TV
    [[WebServer sharedInstance] startUploader];
    [WebServer sharedInstance].webUploader.delegate = self;
#endif
}

#pragma mark GCDWebServerDelegate
- (void)webServerDidCompleteBonjourRegistration:(GCDWebServer*)server {
    NSMutableString *servers = [[NSMutableString alloc] init];
    if ( server.serverURL != nil ) {
        [servers appendString:[NSString stringWithFormat:@"%@",server.serverURL]];
    }
    if ( servers.length > 0 ) {
        [servers appendString:@"\n\n"];
    }
    if ( server.bonjourServerURL != nil ) {
        [servers appendString:[NSString stringWithFormat:@"%@",server.bonjourServerURL]];
    }
#if TARGET_OS_TV
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Welcome to RetroArch" message:[NSString stringWithFormat:@"To transfer files from your computer, go to one of these addresses on your web browser:\n\n%@",servers] preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
    }]];
    [self presentViewController:alert animated:YES completion:^{
    }];
#elif TARGET_OS_IOS
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Web Server Started" message:[NSString stringWithFormat:@"To transfer ROMs from your computer, go to one of these addresses on your web browser:\n\n%@",servers] preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Stop Server" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
        [[WebServer sharedInstance] webUploader].delegate = nil;
        [[WebServer sharedInstance] stopUploader];
    }]];
    [self presentViewController:alert animated:YES completion:^{
    }];
#endif
}
#endif

@end

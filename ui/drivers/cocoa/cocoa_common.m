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

#ifdef HAVE_COCOATOUCH
#import "GCDWebUploader.h"
#import "WebServer.h"
#include "apple_platform.h"
#endif

/* forward declarations */
void cocoagl_gfx_ctx_update(void);

#ifdef HAVE_COCOATOUCH
void *glkitview_init(void);

@interface CocoaView()<GCDWebUploaderDelegate> {

}
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
#elif defined(HAVE_COCOATOUCH)
#if defined(HAVE_COCOA_METAL)
   self.view       = [UIView new];
#else
   self.view       = (BRIDGE GLKView*)glkitview_init();
#endif
#endif
    
#if defined(OSX)
    video_driver_display_type_set(RARCH_DISPLAY_OSX);
    video_driver_display_set(0);
    video_driver_display_userdata_set((uintptr_t)self);
#elif TARGET_OS_IOS
    UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(showNativeMenu)];
    swipe.numberOfTouchesRequired = 4;
    swipe.direction = UISwipeGestureRecognizerDirectionDown;
    [self.view addGestureRecognizer:swipe];
#endif

   return self;
}

#if defined(OSX)
- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];

   cocoagl_gfx_ctx_update();
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
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSURLPboardType])
    {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        NSString    *s = [fileURL path];
        if (s != nil)
        {
           RARCH_LOG("Drop name is: %s\n", [s UTF8String]);
        }
    }
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
      RAScreen *screen                   = (BRIDGE RAScreen*)get_chosen_screen();
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
   float width       = 0.0f, height = 0.0f;
   RAScreen *screen  = (BRIDGE RAScreen*)get_chosen_screen();
   UIInterfaceOrientation orientation = self.interfaceOrientation;
   CGRect screenSize = [screen bounds];
   SEL selector      = NSSelectorFromString(BOXSTRING("coordinateSpace"));

   if ([screen respondsToSelector:selector])
   {
      screenSize  = [[screen coordinateSpace] bounds];
      width       = CGRectGetWidth(screenSize);
      height      = CGRectGetHeight(screenSize);
   }
   else
   {
      width       = ((int)orientation < 3) 
         ? CGRectGetWidth(screenSize) 
         : CGRectGetHeight(screenSize);
      height      = ((int)orientation < 3) 
         ? CGRectGetHeight(screenSize) 
         : CGRectGetWidth(screenSize);
   }

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
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Welcome to RetroArch" message:[NSString stringWithFormat:@"To transfer files from your computer, go to one of these addresses on your web browser:\n\n%@",servers] preferredStyle:UIAlertControllerStyleAlert];
#if TARGET_OS_TV
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
    }]];
#elif TARGET_OS_IOS
    [alert addAction:[UIAlertAction actionWithTitle:@"Stop Server" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
        [[WebServer sharedInstance] webUploader].delegate = nil;
        [[WebServer sharedInstance] stopUploader];
    }]];
#endif
    [self presentViewController:alert animated:YES completion:^{
    }];
#endif
}
#endif

@end

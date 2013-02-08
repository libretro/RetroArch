//
//  AppDelegate.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "AppDelegate.h"
#import "dirlist.h"
#import "browser.h"

#define MAX_TOUCH 16
extern struct
{
   bool is_down;
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} ios_touches[MAX_TOUCH];

extern uint32_t ios_current_touch_count ;

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

   // Override point for customization after application launch.
   if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
      self.window.rootViewController = [[browser alloc] initWithNibName:@"ViewController_iPhone" bundle:nil];
   else
      self.window.rootViewController = [[browser alloc] initWithNibName:@"ViewController_iPad" bundle:nil];
    
   [self.window makeKeyAndVisible];
}

- (void)processTouches:(NSArray*)touches
{
   ios_current_touch_count = [touches count];
   
   for(int i = 0; i != [touches count]; i ++)
   {
      UITouch *touch = [touches objectAtIndex:i];
      CGPoint coord = [touch locationInView:self.window.rootViewController.view];
      float scale = [[UIScreen mainScreen] scale];
      
      ios_touches[i].is_down = (touch.phase != UITouchPhaseEnded) && (touch.phase != UITouchPhaseCancelled);

      ios_touches[i].screen_x = coord.x * scale;
      ios_touches[i].screen_y = coord.y * scale;
   }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
   [super touchesBegan:touches withEvent:event];
   [self processTouches:[[event allTouches] allObjects]];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
   [super touchesMoved:touches withEvent:event];
   [self processTouches:[[event allTouches] allObjects]];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
   [super touchesEnded:touches withEvent:event];
   [self processTouches:[[event allTouches] allObjects]];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
   [super touchesCancelled:touches withEvent:event];
   [self processTouches:[[event allTouches] allObjects]];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
   extern void ios_close_game();
   ios_close_game();
}

@end


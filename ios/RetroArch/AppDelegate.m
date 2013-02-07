//
//  AppDelegate.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "AppDelegate.h"
#import "dirlist.h"

extern bool IOS_is_down;
extern int16_t IOS_touch_x, IOS_touch_y;

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

   // Override point for customization after application launch.
   if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
      self.window.rootViewController = [[dirlist_view alloc] initWithNibName:@"ViewController_iPhone" bundle:nil];
   else
      self.window.rootViewController = [[dirlist_view alloc] initWithNibName:@"ViewController_iPad" bundle:nil];
    
   [self.window makeKeyAndVisible];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
   UITouch *touch = [[event allTouches] anyObject];
   CGPoint coord = [touch locationInView:self.window.rootViewController.view];
   float scale = [[UIScreen mainScreen] scale];
   
   IOS_is_down = true;
   IOS_touch_x = coord.x * scale;
   IOS_touch_y = coord.y * scale;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
   UITouch *touch = [[event allTouches] anyObject];
   CGPoint coord = [touch locationInView:self.window.rootViewController.view];
   IOS_is_down = true;
   IOS_touch_x = coord.x;
   IOS_touch_y = coord.y;
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
   IOS_is_down = false;
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
   IOS_is_down = false;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
   extern void ios_close_game();
   ios_close_game();
}

@end


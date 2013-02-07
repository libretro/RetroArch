//
//  AppDelegate.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "AppDelegate.h"
#import "gameview.h"
#import "dirlist.h"

extern bool IOS_is_down;
extern int16_t IOS_touch_x, IOS_fix_x;
extern int16_t IOS_touch_y, IOS_fix_y;
extern int16_t IOS_full_x, IOS_full_y;


@implementation AppDelegate

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

   // Override point for customization after application launch.
   if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
      self.viewController = [[dirlist_view alloc] initWithNibName:@"ViewController_iPhone" bundle:nil];
   else
      self.viewController = [[dirlist_view alloc] initWithNibName:@"ViewController_iPad" bundle:nil];

   self.window.rootViewController = self.viewController;
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

- (void)applicationWillResignActive:(UIApplication *)application
{
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

@end


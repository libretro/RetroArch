//
//  AppDelegate.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "AppDelegate.h"

#import "gameview.h"

#include "general.h"


extern bool IOS_is_down;
extern int16_t IOS_touch_x, IOS_fix_x;
extern int16_t IOS_touch_y, IOS_fix_y;
extern int16_t IOS_full_x, IOS_full_y;


@implementation AppDelegate
{
    BOOL ra_initialized;
    BOOL ra_paused;
    BOOL ra_done;
}

- (const char*)generate_config
{
   const char* overlay = [[[NSBundle mainBundle] pathForResource:@"overlay" ofType:@"cfg"] UTF8String];
   const char* config = [[NSTemporaryDirectory() stringByAppendingPathComponent: @"retroarch.cfg"] UTF8String];

   FILE* config_file = fopen(config, "wb");
   
   if (config_file)
   {
      if (overlay) fprintf(config_file, "input_overlay = \"%s\"\n", overlay);
      fclose(config_file);
      return config;
   }
   
   return 0;
}

- (void)schedule_iterate
{
   if (ra_initialized && !ra_paused && !ra_done)
   {
      [self performSelector:@selector(rarch_iterate:) withObject:nil afterDelay:0.002f];
   }
}

- (void)rarch_init
{
   const char* filename = [[[NSBundle mainBundle] pathForResource:@"test" ofType:@"img"] UTF8String];
   const char* libretro = [[[NSBundle mainBundle] pathForResource:@"libretro" ofType:@"dylib"] UTF8String];
   const char* config_file = [self generate_config];

   if(!config_file) return;

   const char* argv[] = {"retroarch", "-L", libretro, "-c", config_file, filename, 0};
   if (rarch_main_init(6, (char**)argv) == 0)
   {
      rarch_init_msg_queue();
      ra_initialized = TRUE;
   }
}

- (void)rarch_iterate:(id)sender
{
   if (!ra_paused && ra_initialized && !ra_done)
      ra_done = !rarch_main_iterate();
    
   [self schedule_iterate];
}

- (void)rarch_deinit
{
   if (ra_initialized)
   {
      rarch_main_deinit();
      rarch_deinit_msg_queue();
      
#ifdef PERF_TEST
      rarch_perf_log();
#endif
      
      rarch_main_clear_state();
   }
   
   ra_initialized = FALSE;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   ra_paused = NO;
   ra_done = NO;
   ra_initialized = NO;

   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

   // Override point for customization after application launch.
   if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
      self.viewController = [[game_view alloc] initWithNibName:@"ViewController_iPhone" bundle:nil];
   else
      self.viewController = [[game_view alloc] initWithNibName:@"ViewController_iPad" bundle:nil];

   self.window.rootViewController = self.viewController;
   [self.window makeKeyAndVisible];
   
   [self rarch_init];
   [self performSelector:@selector(rarch_iterate:) withObject:nil afterDelay:1];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
   UITouch *touch = [[event allTouches] anyObject];
   CGPoint coord = [touch locationInView:self.viewController.view];
   float scale = [[UIScreen mainScreen] scale];
   
   IOS_is_down = true;
   IOS_touch_x = coord.x * scale;
   IOS_touch_y = coord.y * scale;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
   UITouch *touch = [[event allTouches] anyObject];
   CGPoint coord = [touch locationInView:self.viewController.view];
   
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
   ra_paused = YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   ra_paused = YES;
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
   if (ra_paused)
   {
      ra_paused = NO;
      [self schedule_iterate];
   }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

@end


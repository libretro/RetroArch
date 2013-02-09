//
//  AppDelegate.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include <sys/stat.h>

#define MAX_TOUCH 16
extern struct
{
   bool is_down;
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} ios_touches[MAX_TOUCH];

extern uint32_t ios_current_touch_count ;

@implementation RetroArch_iOS

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   // TODO: Relocate this!
   self.system_directory = "/var/mobile/Library/RetroArch/";
   mkdir(self.system_directory, 0755);

   bool is_iphone = [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone;
   self.nib_name = is_iphone ? @"ViewController_iPhone" : @"ViewController_iPad";
   
   // Load icons
   self.file_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_file" ofType:@"png"]];
   self.folder_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_dir" ofType:@"png"]];

   // Load buttons
   self.settings_button = [[UIBarButtonItem alloc]
                          initWithTitle:@"Settings"
                          style:UIBarButtonItemStyleBordered
                          target:nil action:nil];


   self.navigator = [[UINavigationController alloc] initWithNibName:self.nib_name bundle:nil];
   [self.navigator pushViewController: [[module_list alloc] initWithNibName:self.nib_name bundle:nil] animated:YES];

   self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   self.window.rootViewController = self.navigator;
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


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

extern bool ios_keys[256];

extern uint32_t ios_current_touch_count;

@implementation RetroArch_iOS
{
   game_view* game;
   
   UIWindow* window;
   UINavigationController* navigator;
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)runGame:(NSString*)path
{
   game = [[game_view alloc] initWithGame:path];
   window.rootViewController = game;
   navigator = nil;
}

- (void)gameHasExited
{
   game = nil;

   navigator = [[UINavigationController alloc] init];
   [navigator pushViewController: [[module_list alloc] init] animated:YES];

   window.rootViewController = navigator;
}

- (void)pushViewController:(UIViewController*)theView
{
   if (navigator != nil)
   {
      [navigator pushViewController:theView animated:YES];
   }
}

- (void)popViewController
{
   if (navigator != nil)
   {
      [navigator popViewControllerAnimated:YES];
   }
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   // TODO: Relocate this!
   self.system_directory = @"/var/mobile/Library/RetroArch/";
   mkdir([self.system_directory UTF8String], 0755);
   
   self.config_file_path = [self.system_directory stringByAppendingPathComponent:@"retroarch.cfg"];
      
   // Load icons
   self.file_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_file" ofType:@"png"]];
   self.folder_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_dir" ofType:@"png"]];

   // Load buttons
   self.settings_button = [[UIBarButtonItem alloc]
                          initWithTitle:@"Settings"
                          style:UIBarButtonItemStyleBordered
                          target:nil action:nil];
   self.settings_button.target = self;
   self.settings_button.action = @selector(show_settings);

   // Setup window
   navigator = [[UINavigationController alloc] init];
   [navigator pushViewController: [[module_list alloc] init] animated:YES];

   window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   window.rootViewController = navigator;
   [window makeKeyAndVisible];
   
   // Setup keyboard hack
   [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(keyPressed:) name: GSEventKeyDownNotification object: nil];
   [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(keyReleased:) name: GSEventKeyUpNotification object: nil];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
   if (game)
   {
      [game resume];
   }
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   if (game)
   {
      [game pause];
   }
}

-(void) keyPressed: (NSNotification*) notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < 256) ios_keys[keycode] = true;
}

-(void) keyReleased: (NSNotification*) notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < 256) ios_keys[keycode] = false;
}

- (void)show_settings
{
   [self pushViewController:[SettingsList new]];
}

- (void)processTouches:(NSArray*)touches
{
   if (game)
   {
      ios_current_touch_count = [touches count];
   
      for(int i = 0; i != [touches count]; i ++)
      {
         UITouch *touch = [touches objectAtIndex:i];
         CGPoint coord = [touch locationInView:game.view];
         float scale = [[UIScreen mainScreen] scale];
      
         // Exit hack!
         if (touch.tapCount == 3)
         {
            if (coord.y < game.view.bounds.size.height / 10.0f)
            {
               float tenpct = game.view.bounds.size.width / 10.0f;
               if (coord.x >= tenpct * 4 && coord.x <= tenpct * 6)
               {
                  [game exit];
               }
            }
         }

         ios_touches[i].is_down = (touch.phase != UITouchPhaseEnded) && (touch.phase != UITouchPhaseCancelled);

         ios_touches[i].screen_x = coord.x * scale;
         ios_touches[i].screen_y = coord.y * scale;
      }
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

@end


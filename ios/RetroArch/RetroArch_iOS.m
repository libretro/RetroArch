/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <sys/stat.h>
#include "rarch_wrapper.h"

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
   UIWindow* _window;
   UINavigationController* _navigator;
}

+ (void)displayErrorMessage:(NSString*)message
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                             message:message
                                             delegate:nil
                                             cancelButtonTitle:@"OK"
                                             otherButtonTitles:nil];
   [alert show];
}

+ (RetroArch_iOS*)get
{
   return (RetroArch_iOS*)[[UIApplication sharedApplication] delegate];
}

- (void)runGame:(NSString*)path
{
   ios_load_game([path UTF8String]);
}

- (void)gameHasExited
{
   _navigator = [[UINavigationController alloc] init];
   [_navigator pushViewController: [[RAModuleList alloc] init] animated:YES];

   _window.rootViewController = _navigator;
}

- (void)pushViewController:(UIViewController*)theView
{
   if (_navigator != nil)
      [_navigator pushViewController:theView animated:YES];
}

- (void)popViewController
{
   if (_navigator != nil)
      [_navigator popViewControllerAnimated:YES];
}

- (void)setViewer:(UIViewController*)theView
{
   _navigator = nil;
   [[UIApplication sharedApplication] setStatusBarHidden:theView ? YES : NO withAnimation:UIStatusBarAnimationSlide];
   _window.rootViewController = theView;
}

- (NSString*)configFilePath
{
   if (self.module_path)
   {
      return [NSString stringWithFormat:@"%@/%@.cfg", self.system_directory, [[self.module_path lastPathComponent] stringByDeletingPathExtension]];
   }
   
   return nil;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
   // TODO: Relocate this!
   self.system_directory = @"/var/mobile/Library/RetroArch/";
   mkdir([self.system_directory UTF8String], 0755);
         
   // Load icons
   self.file_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_file" ofType:@"png"]];
   self.folder_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_dir" ofType:@"png"]];

   // Load buttons
   self.settings_button = [[UIBarButtonItem alloc]
                          initWithTitle:@"Module Settings"
                          style:UIBarButtonItemStyleBordered
                          target:nil action:nil];
   self.settings_button.target = self;
   self.settings_button.action = @selector(show_settings);

   // Setup window
   _navigator = [[UINavigationController alloc] init];
   [_navigator pushViewController: [[RAModuleList alloc] init] animated:YES];

   _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
   _window.rootViewController = _navigator;
   [_window makeKeyAndVisible];
   
   // Setup keyboard hack
   [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(keyPressed:) name: GSEventKeyDownNotification object: nil];
   [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(keyReleased:) name: GSEventKeyUpNotification object: nil];
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
   ios_resume_emulator();
}

- (void)applicationWillResignActive:(UIApplication*)application
{
   ios_pause_emulator();
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
   ios_activate_emulator();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
   ios_suspend_emulator();
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
   [self pushViewController:[RASettingsList new]];
}

- (void)processTouches:(NSArray*)touches
{
   ios_current_touch_count = [touches count];
   
   UIView* view = _window.rootViewController.view;
   
   for(int i = 0; i != [touches count]; i ++)
   {
      UITouch *touch = [touches objectAtIndex:i];
      CGPoint coord = [touch locationInView:view];
      float scale = [[UIScreen mainScreen] scale];
      
      // Exit hack!
      if (touch.tapCount == 3)
      {
         if (coord.y < view.bounds.size.height / 10.0f)
         {
            float tenpct = view.bounds.size.width / 10.0f;
            if (coord.x >= tenpct * 4 && coord.x <= tenpct * 6)
            {
               ios_close_game();
            }
         }
      }

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

@end


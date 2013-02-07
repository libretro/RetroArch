/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#import "gameview.h"
#include "general.h"

static GLKView *gl_view;
static float screen_scale;

@interface game_view ()

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKView *view;

@end

@implementation game_view
{
    BOOL ra_initialized;
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
   if (ra_initialized && !ra_done)
   {
      [self performSelector:@selector(rarch_iterate:) withObject:nil afterDelay:0.002f];
   }
}

- (void)rarch_iterate:(id)sender
{
   if (ra_initialized && !ra_done)
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

- (void)load_game:(const char*)file_name
{
   if(!ra_initialized && file_name)
   {
      const char* libretro = [[[NSBundle mainBundle] pathForResource:@"libretro" ofType:@"dylib"] UTF8String];
      const char* config_file = [self generate_config];

      if(!config_file) return;

      const char* argv[] = {"retroarch", "-L", libretro, "-c", config_file, file_name, 0};
      if (rarch_main_init(6, (char**)argv) == 0)
      {
         rarch_init_msg_queue();
         ra_initialized = TRUE;
         [self schedule_iterate];
      }
   }
}

- (void)viewDidLoad
{
   [super viewDidLoad];

   ra_done = NO;
   ra_initialized = NO;

   self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   self.view = [[GLKView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) context:self.context];
   
   [EAGLContext setCurrentContext:self.context];

   gl_view = self.view;
   screen_scale = [[UIScreen mainScreen] scale];
}

- (void)dealloc
{    
   if ([EAGLContext currentContext] == self.context) [EAGLContext setCurrentContext:nil];
}

@end

void flip_game_view()
{
   [gl_view setNeedsDisplay];
   [gl_view bindDrawable];
}

void get_game_view_size(unsigned *width, unsigned *height)
{
   *width  = gl_view.bounds.size.width * screen_scale;
   *height = gl_view.bounds.size.height * screen_scale;
}


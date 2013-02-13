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

#include "general.h"

static GLKView *gl_view;
static float screen_scale;
static int frame_skips = 4;
static bool is_syncing = true;

@implementation game_view
{
   EAGLContext *gl_context;
   NSString* game;
}

- (id)initWithGame:(NSString*)path
{
   self = [super init];
   game = path;
   screen_scale = [[UIScreen mainScreen] scale];
   
   return self;
}

- (void)dealloc
{
   if ([EAGLContext currentContext] == gl_context) [EAGLContext setCurrentContext:nil];
   gl_context = nil;
   gl_view = nil;
}

- (void)loadView
{
   gl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   [EAGLContext setCurrentContext:gl_context];
   
   gl_view = [[GLKView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) context:gl_context];
   gl_view.multipleTouchEnabled = YES;
   self.view = gl_view;
   
   [self performSelector:@selector(runGame) withObject:nil afterDelay:0.2f];
}

- (void)runGame
{
   [SettingsList refreshConfigFile];
   
   const char* const sd = [[RetroArch_iOS get].system_directory UTF8String];
   const char* const cf =[[RetroArch_iOS get].config_file_path UTF8String];
   const char* const libretro = [[RetroArch_iOS get].module_path UTF8String];

   struct rarch_main_wrap main_wrapper = {[game UTF8String], sd, sd, cf, libretro};
   if (rarch_main_init_wrap(&main_wrapper) == 0)
   {
      rarch_init_msg_queue();
      while (rarch_main_iterate())
         while(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource);
      rarch_main_deinit();
      rarch_deinit_msg_queue();
      
#ifdef PERF_TEST
      rarch_perf_log();
#endif
      
      rarch_main_clear_state();
   }
   
   [[RetroArch_iOS get] gameHasExited];
}

@end

void ios_flip_game_view()
{
   if (gl_view)
   {
      if (--frame_skips < 0)
      {
         [gl_view setNeedsDisplay];
         [gl_view bindDrawable];
         frame_skips = is_syncing ? 0 : 3;
      }
   }
}

void ios_set_game_view_sync(bool on)
{
   is_syncing = on;
   frame_skips = on ? 0 : 3;
}

void ios_get_game_view_size(unsigned *width, unsigned *height)
{
   if (gl_view)
   {
      *width  = gl_view.bounds.size.width * screen_scale;
      *height = gl_view.bounds.size.height * screen_scale;
   }
}

void ios_bind_game_view_fbo()
{
   if (gl_view)
   {
      [gl_view bindDrawable];
   }
}

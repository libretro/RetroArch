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

static game_view *current_view;
static GLKView *gl_view;
static float screen_scale;
static bool ra_initialized = false;
static bool ra_done = false;
static int frame_skips = 4;
static bool is_syncing = true;

void ios_load_game(const char* file_name)
{
   if(!ra_initialized && file_name)
   {
      const char* const sd = [RetroArch_iOS get].system_directory;
      const char* libretro = [[RetroArch_iOS get].module_path UTF8String];

      char config_path[PATH_MAX];
      snprintf(config_path, PATH_MAX, "%s/retroarch.cfg", sd);
      config_path[PATH_MAX - 1] = 0;
      bool have_config = 0 == access(config_path, R_OK);

      struct rarch_main_wrap main_wrapper = {file_name, sd, sd, have_config ? config_path : 0, libretro};
      if (rarch_main_init_wrap(&main_wrapper) == 0)
      {
         rarch_init_msg_queue();
         ra_initialized = TRUE;

         [current_view performSelector:@selector(rarch_iterate:) withObject:nil afterDelay:0.2f];
      }
   }
}

void ios_close_game()
{
   if (ra_initialized)
   {
      rarch_main_deinit();
      rarch_deinit_msg_queue();
      
#ifdef PERF_TEST
      rarch_perf_log();
#endif
      
      rarch_main_clear_state();
      
      ra_done = true;
   }
   
   ra_initialized = false;
}

@implementation game_view
{
   EAGLContext *gl_context;
}

- (void)rarch_iterate:(id)sender
{
   if (ra_initialized && !ra_done)
   {
      while (!ra_done && rarch_main_iterate())
      {
         while(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource);
      }

      ios_close_game();
      
      ra_done = true;
   }
}

- (void)viewDidLoad
{
   [super viewDidLoad];

   gl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   [EAGLContext setCurrentContext:gl_context];
   
   gl_view = [[GLKView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) context:gl_context];
   gl_view.multipleTouchEnabled = YES;
   self.view = gl_view;

   screen_scale = [[UIScreen mainScreen] scale];
   current_view = self;
}

- (void)dealloc
{
   if ([EAGLContext currentContext] == gl_context) [EAGLContext setCurrentContext:nil];
   gl_context = nil;
   gl_view = nil;
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

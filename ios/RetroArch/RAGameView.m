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

#include "general.h"
#include "rarch_wrapper.h"

static float screen_scale;
static int frame_skips = 4;
static bool is_syncing = true;

@implementation RAGameView
{
   EAGLContext* _glContext;
}

- (id)init
{
   self = [super init];
   screen_scale = [[UIScreen mainScreen] scale];
   
   _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
   [EAGLContext setCurrentContext:_glContext];
   
   self.view = [GLKView new];
   ((GLKView*)self.view).context = _glContext;
   self.view.multipleTouchEnabled = YES;

   return self;
}

- (void)needsToDie
{
   glFinish();

   GLKView* glview = (GLKView*)self.view;
   glview.context = nil;
   _glContext = nil;
   [EAGLContext setCurrentContext:nil];
}

@end

static RAGameView* gameViewer;

bool ios_init_game_view()
{
   if (!gameViewer)
   {
      gameViewer = [RAGameView new];
      [[RetroArch_iOS get] pushViewController:gameViewer isGame:YES];
   }
   
   return true;
}

void ios_destroy_game_view()
{
   if (gameViewer)
   {
      [gameViewer needsToDie];
      [[RetroArch_iOS get] popViewController];
      gameViewer = nil;
   }
}

void ios_flip_game_view()
{
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
   
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
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
   
      *width  = gl_view.bounds.size.width * screen_scale;
      *height = gl_view.bounds.size.height * screen_scale;
   }
}

void ios_bind_game_view_fbo()
{
   if (gameViewer)
   {
      GLKView* gl_view = (GLKView*)gameViewer.view;
      [gl_view bindDrawable];
   }
}

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

static GLKView *gl_view;
static float screen_scale;

@interface game_view ()

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKView *view;

@end

@implementation game_view

- (void)viewDidLoad
{
   [super viewDidLoad];

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

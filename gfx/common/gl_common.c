/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <glsym/glsym.h>

void gl_flush(void)
{
   glFlush();
}

void gl_clear(void)
{
   glClear(GL_COLOR_BUFFER_BIT);
}

void gl_disable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glDisable(cap);
}

void gl_enable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glEnable(cap);
}

void gl_finish(void)
{
   glFinish();
}

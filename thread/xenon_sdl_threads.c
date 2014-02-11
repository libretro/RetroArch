/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

// libSDLxenon doesn't implement this yet :[. Implement it very stupidly for now. ;)

#include "SDL_thread.h"
#include "SDL_mutex.h"
#include <stdlib.h>
#include "../boolean.h"

SDL_cond *SDL_CreateCond(void)
{
   bool *sleeping = calloc(1, sizeof(*sleeping));
   return (SDL_cond*)sleeping;
}

void SDL_DestroyCond(SDL_cond *sleeping)
{
   free(sleeping);
}

int SDL_CondWait(SDL_cond *cond, SDL_mutex *lock)
{
   (void)lock;
   volatile bool *sleeping = (volatile bool*)cond;

   SDL_mutexV(lock);
   *sleeping = true;
   while (*sleeping); // Yeah, we all love busyloops don't we? ._.
   SDL_mutexP(lock);

   return 0;
}

int SDL_CondSignal(SDL_cond *cond)
{
   *(volatile bool*)cond = false;
   return 0;
}


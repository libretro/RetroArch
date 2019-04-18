/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xenon_sdl_threads.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* libSDLxenon doesn't implement this yet :[. Implement it very stupidly for now. ;) */

#include "SDL_thread.h"
#include "SDL_mutex.h"
#include <stdlib.h>
#include <boolean.h>

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
   while (*sleeping); /* Yeah, we all love busyloops don't we? ._. */
   SDL_mutexP(lock);

   return 0;
}

int SDL_CondSignal(SDL_cond *cond)
{
   *(volatile bool*)cond = false;
   return 0;
}

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

#include <stdlib.h>
#include <stdio.h>
#include "tween.h"

tween* tweens = NULL;
int numtweens = 0;

float inOutQuad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

void add_tween(float duration, float target_value, float* subject, easingFunc easing) {
   numtweens++;
   tweens = realloc(tweens, numtweens * sizeof(tween));
   tweens[numtweens-1].alive = 1;
   tweens[numtweens-1].duration = duration;
   tweens[numtweens-1].running_since = 0;
   tweens[numtweens-1].initial_value = *subject;
   tweens[numtweens-1].target_value = target_value;
   tweens[numtweens-1].subject = subject;
   tweens[numtweens-1].easing = easing;
}

void update_tweens(float dt)
{
   int active_tweens = 0;
   for(int i = 0; i < numtweens; i++)
   {
      tweens[i] = update_tween(tweens[i], dt);
      active_tweens += tweens[i].running_since < tweens[i].duration ? 1 : 0;
   }
   if (numtweens && !active_tweens) {
      numtweens = 0;
   }
}

tween update_tween(tween tw, float dt)
{
   if (tw.running_since < tw.duration) {
      tw.running_since += dt;
      *(tw.subject) = tw.easing(
         tw.running_since,
         tw.initial_value,
         tw.target_value - tw.initial_value,
         tw.duration);
      if (tw.running_since >= tw.duration)
         *(tw.subject) = tw.target_value;
   }
   return tw;
}

void free_tweens()
{
   free(tweens);
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2014      - Jean-André Santoni
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

#ifndef _TWEEN_H
#define _TWEEN_H

#include <stdlib.h>

typedef float (*easingFunc)(float, float, float, float);

typedef void (*tweenCallback) (void);

typedef struct
{
   int    alive;
   float  duration;
   float  running_since;
   float  initial_value;
   float  target_value;
   float* subject;
   easingFunc easing;
   tweenCallback callback;
} tween_t;

void add_tween(float duration, float target_value,
      float* subject, easingFunc easing, tweenCallback callback);

void update_tweens(float dt);

// from https://github.com/kikito/tween.lua/blob/master/tween.lua

float linear(float t, float b, float c, float d);
float inQuad(float t, float b, float c, float d);
float outQuad(float t, float b, float c, float d);
float inOutQuad(float t, float b, float c, float d);
float outInQuad(float t, float b, float c, float d);
float inCubic(float t, float b, float c, float d);
float outCubic(float t, float b, float c, float d);
float inOutCubic(float t, float b, float c, float d);
float outInCubic(float t, float b, float c, float d);
float inQuart(float t, float b, float c, float d);
float outQuart(float t, float b, float c, float d);
float inOutQuart(float t, float b, float c, float d);
float outInQuart(float t, float b, float c, float d);
float inQuint(float t, float b, float c, float d);
float outQuint(float t, float b, float c, float d);
float inOutQuint(float t, float b, float c, float d);
float outInQuint(float t, float b, float c, float d);
float inSine(float t, float b, float c, float d);
float outSine(float t, float b, float c, float d);
float inOutSine(float t, float b, float c, float d);
float outInSine(float t, float b, float c, float d);
float inExpo(float t, float b, float c, float d);
float outExpo(float t, float b, float c, float d);
float inOutExpo(float t, float b, float c, float d);
float outInExpo(float t, float b, float c, float d);
float inCirc(float t, float b, float c, float d);
float outCirc(float t, float b, float c, float d);
float inOutCirc(float t, float b, float c, float d);
float outInCirc(float t, float b, float c, float d);
float inBounce(float t, float b, float c, float d);
float outBounce(float t, float b, float c, float d);
float inOutBounce(float t, float b, float c, float d);
float outInBounce(float t, float b, float c, float d);

#endif /* TWEEN_H */

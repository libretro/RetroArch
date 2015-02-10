/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "menu_animation.h"
#include "../driver.h"
#include <math.h>

void menu_animation_free(animation_t *animation)
{
   size_t i;

   if (!animation)
      return;

   for (i = 0; i < animation->size; i++)
   {
      free(animation->list[i].subject);
   }

   free(animation->list);
   free(animation);
}

bool menu_animation_push(animation_t *animation,
      float duration, float target_value, float* subject,
      easingFunc easing, tween_cb cb)
{
   if (animation->size >= animation->capacity)
   {
      animation->capacity++;
      animation->list = (struct tween*)realloc(animation->list,
            animation->capacity * sizeof(struct tween));
   }

   animation->list[animation->size].alive         = 1;
   animation->list[animation->size].duration      = duration;
   animation->list[animation->size].running_since = 0;
   animation->list[animation->size].initial_value = *subject;
   animation->list[animation->size].target_value  = target_value;
   animation->list[animation->size].subject       = subject;
   animation->list[animation->size].easing        = easing;
   animation->list[animation->size].cb            = cb;

   animation->size++;

   return true;
}

static int menu_animation_iterate(struct tween *tween, float dt,
      unsigned *active_tweens)
{
   if (!tween)
      return -1;
   if (tween->running_since >= tween->duration)
      return -1;

   tween->running_since += dt;

   if (tween->easing)
      *tween->subject = tween->easing(
            tween->running_since,
            tween->initial_value,
            tween->target_value - tween->initial_value,
            tween->duration);

   if (tween->running_since >= tween->duration)
   {
      *tween->subject = tween->target_value;

      if (tween->cb)
         tween->cb();
   }

   if (tween->running_since < tween->duration)
      *active_tweens += 1;

   return 0;
}

void menu_animation_update(animation_t *animation, float dt)
{
   unsigned i;
   unsigned active_tweens = 0;

   for(i = 0; i < animation->size; i++)
      menu_animation_iterate(&animation->list[i], dt, &active_tweens);

   if (!active_tweens)
      animation->size = 0;
}

/* Linear */

float linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

/* Quad */

float inQuad(float t, float b, float c, float d)
{
   return c * pow(t / d, 2) + b;
}

float outQuad(float t, float b, float c, float d)
{
   t = t / d;
   return -c * t * (t - 2) + b;
}

float inOutQuad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

float outInQuad(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outQuad(t * 2, b, c / 2, d);
   return inQuad((t * 2) - d, b + c / 2, c / 2, d);
}

/* Cubic */

float inCubic(float t, float b, float c, float d)
{
   return c * pow(t / d, 3) + b;
}

float outCubic(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 3) + 1) + b;
}

float inOutCubic(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * t * t * t + b;
   t = t - 2;
   return c / 2 * (t * t * t + 2) + b;
}

float outInCubic(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outCubic(t * 2, b, c / 2, d);
   return inCubic((t * 2) - d, b + c / 2, c / 2, d);
}

/* Quart */

float inQuart(float t, float b, float c, float d)
{
   return c * pow(t / d, 4) + b;
}

float outQuart(float t, float b, float c, float d)
{
   return -c * (pow(t / d - 1, 4) - 1) + b;
}

float inOutQuart(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 4) + b;
   return -c / 2 * (pow(t - 2, 4) - 2) + b;
}

float outInQuart(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outQuart(t * 2, b, c / 2, d);
   return inQuart((t * 2) - d, b + c / 2, c / 2, d);
}

/* Quint */

float inQuint(float t, float b, float c, float d)
{
   return c * pow(t / d, 5) + b;
}

float outQuint(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 5) + 1) + b;
}

float inOutQuint(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 5) + b;
   return c / 2 * (pow(t - 2, 5) + 2) + b;
}

float outInQuint(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outQuint(t * 2, b, c / 2, d);
   return inQuint((t * 2) - d, b + c / 2, c / 2, d);
}

/* Sine */

float inSine(float t, float b, float c, float d)
{
   return -c * cos(t / d * (M_PI / 2)) + c + b;
}

float outSine(float t, float b, float c, float d)
{
   return c * sin(t / d * (M_PI / 2)) + b;
}

float inOutSine(float t, float b, float c, float d)
{
   return -c / 2 * (cos(M_PI * t / d) - 1) + b;
}

float outInSine(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outSine(t * 2, b, c / 2, d);
   return inSine((t * 2) -d, b + c / 2, c / 2, d);
}

/* Expo */

float inExpo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   return c * powf(2, 10 * (t / d - 1)) + b - c * 0.001;
}

float outExpo(float t, float b, float c, float d)
{
   if (t == d)
      return b + c;
   return c * 1.001 * (-powf(2, -10 * t / d) + 1) + b;
}

float inOutExpo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   if (t == d)
      return b + c;
   t = t / d * 2;
   if (t < 1)
      return c / 2 * powf(2, 10 * (t - 1)) + b - c * 0.0005;
   return c / 2 * 1.0005 * (-powf(2, -10 * (t - 1)) + 2) + b;
}

float outInExpo(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outExpo(t * 2, b, c / 2, d);
   return inExpo((t * 2) - d, b + c / 2, c / 2, d);
}

/* Circ */

float inCirc(float t, float b, float c, float d)
{
   return(-c * (sqrt(1 - powf(t / d, 2)) - 1) + b);
}

float outCirc(float t, float b, float c, float d)
{
   return(c * sqrt(1 - powf(t / d - 1, 2)) + b);
}

float inOutCirc(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return -c / 2 * (sqrt(1 - t * t) - 1) + b;
   t = t - 2;
   return c / 2 * (sqrt(1 - t * t) + 1) + b;
}

float outInCirc(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outCirc(t * 2, b, c / 2, d);
   return inCirc((t * 2) - d, b + c / 2, c / 2, d);
}

/* Bounce */

float outBounce(float t, float b, float c, float d)
{
   t = t / d;
   if (t < 1 / 2.75)
      return c * (7.5625 * t * t) + b;
   if (t < 2 / 2.75)
   {
      t = t - (1.5 / 2.75);
      return c * (7.5625 * t * t + 0.75) + b;
   }
   else if (t < 2.5 / 2.75)
   {
      t = t - (2.25 / 2.75);
      return c * (7.5625 * t * t + 0.9375) + b;
   }
   t = t - (2.625 / 2.75);
   return c * (7.5625 * t * t + 0.984375) + b;
}

float inBounce(float t, float b, float c, float d)
{
   return c - outBounce(d - t, 0, c, d) + b;
}

float inOutBounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return inBounce(t * 2, 0, c, d) * 0.5 + b;
   return outBounce(t * 2 - d, 0, c, d) * 0.5 + c * .5 + b;
}

float outInBounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outBounce(t * 2, b, c / 2, d);
   return inBounce((t * 2) - d, b + c / 2, c / 2, d);
}

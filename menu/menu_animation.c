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
#include <math.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include "../runloop.h"

/* from https://github.com/kikito/tween.lua/blob/master/tween.lua */

static float easing_linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

static float easing_in_out_quad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

static float easing_in_quad(float t, float b, float c, float d)
{
   return c * pow(t / d, 2) + b;
}

static float easing_out_quad(float t, float b, float c, float d)
{
   t = t / d;
   return -c * t * (t - 2) + b;
}

static float easing_out_in_quad(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quad(t * 2, b, c / 2, d);
   return easing_in_quad((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_cubic(float t, float b, float c, float d)
{
   return c * pow(t / d, 3) + b;
}

static float easing_out_cubic(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 3) + 1) + b;
}

static float easing_in_out_cubic(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * t * t * t + b;
   t = t - 2;
   return c / 2 * (t * t * t + 2) + b;
}

static float easing_out_in_cubic(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_cubic(t * 2, b, c / 2, d);
   return easing_in_cubic((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quart(float t, float b, float c, float d)
{
   return c * pow(t / d, 4) + b;
}

static float easing_out_quart(float t, float b, float c, float d)
{
   return -c * (pow(t / d - 1, 4) - 1) + b;
}

static float easing_in_out_quart(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 4) + b;
   return -c / 2 * (pow(t - 2, 4) - 2) + b;
}

static float easing_out_in_quart(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quart(t * 2, b, c / 2, d);
   return easing_in_quart((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quint(float t, float b, float c, float d)
{
   return c * pow(t / d, 5) + b;
}

static float easing_out_quint(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 5) + 1) + b;
}

static float easing_in_out_quint(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 5) + b;
   return c / 2 * (pow(t - 2, 5) + 2) + b;
}

static float easing_out_in_quint(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quint(t * 2, b, c / 2, d);
   return easing_in_quint((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_sine(float t, float b, float c, float d)
{
   return -c * cos(t / d * (M_PI / 2)) + c + b;
}

static float easing_out_sine(float t, float b, float c, float d)
{
   return c * sin(t / d * (M_PI / 2)) + b;
}

static float easing_in_out_sine(float t, float b, float c, float d)
{
   return -c / 2 * (cos(M_PI * t / d) - 1) + b;
}

static float easing_out_in_sine(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_sine(t * 2, b, c / 2, d);
   return easing_in_sine((t * 2) -d, b + c / 2, c / 2, d);
}

static float easing_in_expo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   return c * powf(2, 10 * (t / d - 1)) + b - c * 0.001;
}

static float easing_out_expo(float t, float b, float c, float d)
{
   if (t == d)
      return b + c;
   return c * 1.001 * (-powf(2, -10 * t / d) + 1) + b;
}

static float easing_in_out_expo(float t, float b, float c, float d)
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

static float easing_out_in_expo(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_expo(t * 2, b, c / 2, d);
   return easing_in_expo((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_circ(float t, float b, float c, float d)
{
   return(-c * (sqrt(1 - powf(t / d, 2)) - 1) + b);
}

static float easing_out_circ(float t, float b, float c, float d)
{
   return(c * sqrt(1 - powf(t / d - 1, 2)) + b);
}

static float easing_in_out_circ(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return -c / 2 * (sqrt(1 - t * t) - 1) + b;
   t = t - 2;
   return c / 2 * (sqrt(1 - t * t) + 1) + b;
}

static float easing_out_in_circ(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_circ(t * 2, b, c / 2, d);
   return easing_in_circ((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_out_bounce(float t, float b, float c, float d)
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

static float easing_in_bounce(float t, float b, float c, float d)
{
   return c - easing_out_bounce(d - t, 0, c, d) + b;
}

static float easing_in_out_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_in_bounce(t * 2, 0, c, d) * 0.5 + b;
   return easing_out_bounce(t * 2 - d, 0, c, d) * 0.5 + c * .5 + b;
}

static float easing_out_in_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_bounce(t * 2, b, c / 2, d);
   return easing_in_bounce((t * 2) - d, b + c / 2, c / 2, d);
}

void menu_animation_free(animation_t *animation)
{
   size_t i;

   if (!animation)
      return;

   for (i = 0; i < animation->size; i++)
   {
      if (animation->list[i].subject)
         animation->list[i].subject = NULL;
   }

   free(animation->list);
   free(animation);
}

bool menu_animation_push(animation_t *animation,
      float duration, float target_value, float* subject,
      enum animation_easing_type easing_enum, tween_cb cb)
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
   animation->list[animation->size].cb            = cb;

   switch (easing_enum)
   {
      case EASING_LINEAR:
         animation->list[animation->size].easing        = &easing_linear;
         break;
         /* Quad */
      case EASING_IN_QUAD:
         animation->list[animation->size].easing        = &easing_in_quad;
         break;
      case EASING_OUT_QUAD:
         animation->list[animation->size].easing        = &easing_out_quad;
         break;
      case EASING_IN_OUT_QUAD:
         animation->list[animation->size].easing        = &easing_in_out_quad;
         break;
      case EASING_OUT_IN_QUAD:
         animation->list[animation->size].easing        = &easing_out_in_quad;
         break;
         /* Cubic */
      case EASING_IN_CUBIC:
         animation->list[animation->size].easing        = &easing_in_cubic;
         break;
      case EASING_OUT_CUBIC:
         animation->list[animation->size].easing        = &easing_out_cubic;
         break;
      case EASING_IN_OUT_CUBIC:
         animation->list[animation->size].easing        = &easing_in_out_cubic;
         break;
      case EASING_OUT_IN_CUBIC:
         animation->list[animation->size].easing        = &easing_out_in_cubic;
         break;
         /* Quart */
      case EASING_IN_QUART:
         animation->list[animation->size].easing        = &easing_in_quart;
         break;
      case EASING_OUT_QUART:
         animation->list[animation->size].easing        = &easing_out_quart;
         break;
      case EASING_IN_OUT_QUART:
         animation->list[animation->size].easing        = &easing_in_out_quart;
         break;
      case EASING_OUT_IN_QUART:
         animation->list[animation->size].easing        = &easing_out_in_quart;
         break;
         /* Quint */
      case EASING_IN_QUINT:
         animation->list[animation->size].easing        = &easing_in_quint;
         break;
      case EASING_OUT_QUINT:
         animation->list[animation->size].easing        = &easing_out_quint;
         break;
      case EASING_IN_OUT_QUINT:
         animation->list[animation->size].easing        = &easing_in_out_quint;
         break;
      case EASING_OUT_IN_QUINT:
         animation->list[animation->size].easing        = &easing_out_in_quint;
         break;
         /* Sine */
      case EASING_IN_SINE:
         animation->list[animation->size].easing        = &easing_in_sine;
         break;
      case EASING_OUT_SINE:
         animation->list[animation->size].easing        = &easing_out_sine;
         break;
      case EASING_IN_OUT_SINE:
         animation->list[animation->size].easing        = &easing_in_out_sine;
         break;
      case EASING_OUT_IN_SINE:
         animation->list[animation->size].easing        = &easing_out_in_sine;
         break;
         /* Expo */
      case EASING_IN_EXPO:
         animation->list[animation->size].easing        = &easing_in_expo;
         break;
      case EASING_OUT_EXPO:
         animation->list[animation->size].easing        = &easing_out_expo;
         break;
      case EASING_IN_OUT_EXPO:
         animation->list[animation->size].easing        = &easing_in_out_expo;
         break;
      case EASING_OUT_IN_EXPO:
         animation->list[animation->size].easing        = &easing_out_in_expo;
         break;
         /* Circ */
      case EASING_IN_CIRC:
         animation->list[animation->size].easing        = &easing_in_circ;
         break;
      case EASING_OUT_CIRC:
         animation->list[animation->size].easing        = &easing_out_circ;
         break;
      case EASING_IN_OUT_CIRC:
         animation->list[animation->size].easing        = &easing_in_out_circ;
         break;
      case EASING_OUT_IN_CIRC:
         animation->list[animation->size].easing        = &easing_out_in_circ;
         break;
         /* Bounce */
      case EASING_IN_BOUNCE:
         animation->list[animation->size].easing        = &easing_in_bounce;
         break;
      case EASING_OUT_BOUNCE:
         animation->list[animation->size].easing        = &easing_out_bounce;
         break;
      case EASING_IN_OUT_BOUNCE:
         animation->list[animation->size].easing        = &easing_in_out_bounce;
         break;
      case EASING_OUT_IN_BOUNCE:
         animation->list[animation->size].easing        = &easing_out_in_bounce;
         break;
      default:
         animation->list[animation->size].easing        = NULL;
         break;
   }

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

bool menu_animation_update(animation_t *animation, float dt)
{
   unsigned i;
   unsigned active_tweens = 0;
   menu_handle_t *menu = menu_driver_get_ptr();

   for(i = 0; i < animation->size; i++)
      menu_animation_iterate(&animation->list[i], dt, &active_tweens);

   if (!active_tweens)
   {
      animation->size = 0;
      return false;
   }

   menu->animation_is_active = true;

   return true;
}

/**
 * menu_animation_ticker_line:
 * @buf                      : buffer to write new message line to.
 * @len                      : length of buffer @input.
 * @idx                      : Index. Will be used for ticker logic.
 * @str                      : Input string.
 * @selected                 : Is the item currently selected in the menu?
 *
 * Take the contents of @str and apply a ticker effect to it,
 * and write the results in @buf.
 **/
void menu_animation_ticker_line(char *buf, size_t len, uint64_t idx,
      const char *str, bool selected)
{
   unsigned ticker_period, phase, phase_left_stop;
   unsigned phase_left_moving, phase_right_stop;
   unsigned left_offset, right_offset;
   size_t str_len = strlen(str);
   menu_handle_t *menu = menu_driver_get_ptr();

   if (str_len <= len)
   {
      strlcpy(buf, str, len + 1);
      return;
   }

   if (!selected)
   {
      strlcpy(buf, str, len + 1 - 3);
      strlcat(buf, "...", len + 1);
      return;
   }

   /* Wrap long strings in options with some kind of ticker line. */
   ticker_period = 2 * (str_len - len) + 4;
   phase = idx % ticker_period;

   phase_left_stop = 2;
   phase_left_moving = phase_left_stop + (str_len - len);
   phase_right_stop = phase_left_moving + 2;

   left_offset = phase - phase_left_stop;
   right_offset = (str_len - len) - (phase - phase_right_stop);

   /* Ticker period:
    * [Wait at left (2 ticks),
    * Progress to right(type_len - w),
    * Wait at right (2 ticks),
    * Progress to left].
    */
   if (phase < phase_left_stop)
      strlcpy(buf, str, len + 1);
   else if (phase < phase_left_moving)
      strlcpy(buf, str + left_offset, len + 1);
   else if (phase < phase_right_stop)
      strlcpy(buf, str + str_len - len, len + 1);
   else
      strlcpy(buf, str + right_offset, len + 1);

   menu->animation_is_active = true;
}

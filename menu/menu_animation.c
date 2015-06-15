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

#include <math.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include "menu_display.h"
#include "menu_animation.h"
#include "../configuration.h"
#include "../runloop.h"
#include "../performance.h"

menu_animation_t *menu_animation_get_ptr(void)
{
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp)
      return NULL;
   return disp->animation;
}

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

void menu_animation_free(menu_animation_t *anim)
{
   size_t i;

   if (!anim)
      return;

   for (i = 0; i < anim->size; i++)
   {
      if (anim->list[i].subject)
         anim->list[i].subject = NULL;
   }

   free(anim->list);
   free(anim);
}

void menu_animation_kill_by_subject(menu_animation_t *animation, size_t count, const void *subjects)
{
   unsigned i, j, killed = 0;
   float **sub = (float**)subjects;

   for (i = 0; i < animation->size; ++i)
   {
      if (!animation->list[i].alive)
         continue;

      for (j = 0; j < count; ++j)
      {
         if (animation->list[i].subject == sub[j])
         {
            animation->list[i].alive   = 0;
            animation->list[i].subject = NULL;
            killed++;
            break;
         }
      }
   }
}

bool menu_animation_push(
      menu_animation_t *anim,
      float duration,
      float target_value,
      float* subject,
      enum menu_animation_easing_type easing_enum,
      tween_cb cb)
{
   if (anim->size >= anim->capacity)
   {
      anim->capacity++;
      anim->list = (struct tween*)realloc(anim->list,
            anim->capacity * sizeof(struct tween));
   }

   anim->list[anim->size].alive         = 1;
   anim->list[anim->size].duration      = duration;
   anim->list[anim->size].running_since = 0;
   anim->list[anim->size].initial_value = *subject;
   anim->list[anim->size].target_value  = target_value;
   anim->list[anim->size].subject       = subject;
   anim->list[anim->size].cb            = cb;

   switch (easing_enum)
   {
      case EASING_LINEAR:
         anim->list[anim->size].easing        = &easing_linear;
         break;
         /* Quad */
      case EASING_IN_QUAD:
         anim->list[anim->size].easing        = &easing_in_quad;
         break;
      case EASING_OUT_QUAD:
         anim->list[anim->size].easing        = &easing_out_quad;
         break;
      case EASING_IN_OUT_QUAD:
         anim->list[anim->size].easing        = &easing_in_out_quad;
         break;
      case EASING_OUT_IN_QUAD:
         anim->list[anim->size].easing        = &easing_out_in_quad;
         break;
         /* Cubic */
      case EASING_IN_CUBIC:
         anim->list[anim->size].easing        = &easing_in_cubic;
         break;
      case EASING_OUT_CUBIC:
         anim->list[anim->size].easing        = &easing_out_cubic;
         break;
      case EASING_IN_OUT_CUBIC:
         anim->list[anim->size].easing        = &easing_in_out_cubic;
         break;
      case EASING_OUT_IN_CUBIC:
         anim->list[anim->size].easing        = &easing_out_in_cubic;
         break;
         /* Quart */
      case EASING_IN_QUART:
         anim->list[anim->size].easing        = &easing_in_quart;
         break;
      case EASING_OUT_QUART:
         anim->list[anim->size].easing        = &easing_out_quart;
         break;
      case EASING_IN_OUT_QUART:
         anim->list[anim->size].easing        = &easing_in_out_quart;
         break;
      case EASING_OUT_IN_QUART:
         anim->list[anim->size].easing        = &easing_out_in_quart;
         break;
         /* Quint */
      case EASING_IN_QUINT:
         anim->list[anim->size].easing        = &easing_in_quint;
         break;
      case EASING_OUT_QUINT:
         anim->list[anim->size].easing        = &easing_out_quint;
         break;
      case EASING_IN_OUT_QUINT:
         anim->list[anim->size].easing        = &easing_in_out_quint;
         break;
      case EASING_OUT_IN_QUINT:
         anim->list[anim->size].easing        = &easing_out_in_quint;
         break;
         /* Sine */
      case EASING_IN_SINE:
         anim->list[anim->size].easing        = &easing_in_sine;
         break;
      case EASING_OUT_SINE:
         anim->list[anim->size].easing        = &easing_out_sine;
         break;
      case EASING_IN_OUT_SINE:
         anim->list[anim->size].easing        = &easing_in_out_sine;
         break;
      case EASING_OUT_IN_SINE:
         anim->list[anim->size].easing        = &easing_out_in_sine;
         break;
         /* Expo */
      case EASING_IN_EXPO:
         anim->list[anim->size].easing        = &easing_in_expo;
         break;
      case EASING_OUT_EXPO:
         anim->list[anim->size].easing        = &easing_out_expo;
         break;
      case EASING_IN_OUT_EXPO:
         anim->list[anim->size].easing        = &easing_in_out_expo;
         break;
      case EASING_OUT_IN_EXPO:
         anim->list[anim->size].easing        = &easing_out_in_expo;
         break;
         /* Circ */
      case EASING_IN_CIRC:
         anim->list[anim->size].easing        = &easing_in_circ;
         break;
      case EASING_OUT_CIRC:
         anim->list[anim->size].easing        = &easing_out_circ;
         break;
      case EASING_IN_OUT_CIRC:
         anim->list[anim->size].easing        = &easing_in_out_circ;
         break;
      case EASING_OUT_IN_CIRC:
         anim->list[anim->size].easing        = &easing_out_in_circ;
         break;
         /* Bounce */
      case EASING_IN_BOUNCE:
         anim->list[anim->size].easing        = &easing_in_bounce;
         break;
      case EASING_OUT_BOUNCE:
         anim->list[anim->size].easing        = &easing_out_bounce;
         break;
      case EASING_IN_OUT_BOUNCE:
         anim->list[anim->size].easing        = &easing_in_out_bounce;
         break;
      case EASING_OUT_IN_BOUNCE:
         anim->list[anim->size].easing        = &easing_out_in_bounce;
         break;
      default:
         anim->list[anim->size].easing        = NULL;
         break;
   }

   anim->size++;

   return true;
}

static int menu_animation_iterate(
      struct tween *tween,
      float dt,
      unsigned *active_tweens)
{
   if (!tween)
      return -1;
   if (tween->running_since >= tween->duration || !tween->alive)
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
      tween->alive    = 0;

      if (tween->cb)
         tween->cb();
   }

   if (tween->running_since < tween->duration)
      *active_tweens += 1;

   return 0;
}

bool menu_animation_update(menu_animation_t *anim, float dt)
{
   unsigned i;
   unsigned active_tweens = 0;

   for(i = 0; i < anim->size; i++)
      menu_animation_iterate(&anim->list[i], dt, &active_tweens);

   if (!active_tweens)
   {
      anim->size = 0;
      return false;
   }

   anim->is_active = true;

   return true;
}

/**
 * menu_animation_ticker_line:
 * @s                        : buffer to write new message line to.
 * @len                      : length of buffer @input.
 * @idx                      : Index. Will be used for ticker logic.
 * @str                      : Input string.
 * @selected                 : Is the item currently selected in the menu?
 *
 * Take the contents of @str and apply a ticker effect to it,
 * and write the results in @s.
 **/
void menu_animation_ticker_line(char *s, size_t len, uint64_t idx,
      const char *str, bool selected)
{
   unsigned ticker_period, phase, phase_left_stop;
   unsigned phase_left_moving, phase_right_stop;
   unsigned left_offset, right_offset;
   size_t         str_len = strlen(str);
   menu_animation_t *anim = menu_animation_get_ptr();

   if (str_len <= len)
   {
      strlcpy(s, str, len + 1);
      return;
   }

   if (!selected)
   {
      strlcpy(s, str, len + 1 - 3);
      strlcat(s, "...", len + 1);
      return;
   }

   /* Wrap long strings in options with some kind of ticker line. */
   ticker_period     = 2 * (str_len - len) + 4;
   phase             = idx % ticker_period;

   phase_left_stop   = 2;
   phase_left_moving = phase_left_stop + (str_len - len);
   phase_right_stop  = phase_left_moving + 2;

   left_offset       = phase - phase_left_stop;
   right_offset      = (str_len - len) - (phase - phase_right_stop);

   /* Ticker period:
    * [Wait at left (2 ticks),
    * Progress to right(type_len - w),
    * Wait at right (2 ticks),
    * Progress to left].
    */
   if (phase < phase_left_stop)
      strlcpy(s, str, len + 1);
   else if (phase < phase_left_moving)
      strlcpy(s, str + left_offset, len + 1);
   else if (phase < phase_right_stop)
      strlcpy(s, str + str_len - len, len + 1);
   else
      strlcpy(s, str + right_offset, len + 1);

   anim->is_active = true;
}

void menu_animation_update_time(menu_animation_t *anim)
{
   static retro_time_t last_clock_update = 0;
   settings_t *settings = config_get_ptr();

   anim->cur_time   = rarch_get_time_usec();
   anim->delta_time = anim->cur_time - anim->old_time;

   if (anim->delta_time >= IDEAL_DT * 4)
      anim->delta_time = IDEAL_DT * 4;
   if (anim->delta_time <= IDEAL_DT / 4)
      anim->delta_time = IDEAL_DT / 4;
   anim->old_time      = anim->cur_time;

   if (anim->cur_time - last_clock_update > 1000000 && settings->menu.timedate_enable)
   {
      anim->label.is_updated = true;
      last_clock_update = anim->cur_time;
   }
}

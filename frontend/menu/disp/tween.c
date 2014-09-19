#include "tween.h"
#include <math.h>

tween_t* tweens = NULL;
int numtweens = 0;

void add_tween(float duration, float target_value, float* subject,
      easingFunc easing, tweenCallback callback)
{
   tween_t *tween;
   tween_t *tweens_tmp;

   numtweens++;

   tweens_tmp = (tween_t*)realloc(tweens, numtweens * sizeof(tween_t));
   if (tweens_tmp != NULL)
   {
       tweens = tweens_tmp;
   }
   else // realloc failed
   {
      if (tweens != NULL)
      {
         free(tweens);
         tweens = NULL;
      }

      return;
   }
   
   tween = (tween_t*)&tweens[numtweens-1];

   if (!tween)
      return;

   tween->alive = 1;
   tween->duration = duration;
   tween->running_since = 0;
   tween->initial_value = *subject;
   tween->target_value = target_value;
   tween->subject = subject;
   tween->easing = easing;
   tween->callback = callback;
}

void update_tween(void *data, float dt)
{
   tween_t *tween = (tween_t*)data;

   if (!tween)
      return;

   if (tween->running_since < tween->duration)
   {
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

         if (tween->callback)
            tween->callback();
      }
   }
}

void update_tweens(float dt)
{
   int i, active_tweens;

   active_tweens = 0;

   for(i = 0; i < numtweens; i++)
   {
      update_tween(&tweens[i], dt);
      active_tweens += tweens[i].running_since < tweens[i].duration ? 1 : 0;
   }

   if (numtweens && !active_tweens)
      numtweens = 0;
}

// linear

float linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

// quad

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

// cubic

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

// quart

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

// quint

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

// sine

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

// expo

float inExpo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   return c * pow(2, 10 * (t / d - 1)) + b - c * 0.001;
}

float outExpo(float t, float b, float c, float d)
{
   if (t == d)
      return b + c;
   return c * 1.001 * (-pow(2, -10 * t / d) + 1) + b;
}

float inOutExpo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   if (t == d)
      return b + c;
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(2, 10 * (t - 1)) + b - c * 0.0005;
   return c / 2 * 1.0005 * (-pow(2, -10 * (t - 1)) + 2) + b;
}

float outInExpo(float t, float b, float c, float d)
{
   if (t < d / 2)
      return outExpo(t * 2, b, c / 2, d);
   return inExpo((t * 2) - d, b + c / 2, c / 2, d);
}

// circ

float inCirc(float t, float b, float c, float d)
{
   return(-c * (sqrt(1 - pow(t / d, 2)) - 1) + b);
}

float outCirc(float t, float b, float c, float d)
{
   return(c * sqrt(1 - pow(t / d - 1, 2)) + b);
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

// bounce

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

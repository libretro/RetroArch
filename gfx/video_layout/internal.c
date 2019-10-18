#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <compat/posix_string.h>
#include "internal.h"

char *init_string(const char *src)
{
   return src ? strdup(src) : NULL;
}

void set_string(char **string, const char *src)
{
   free(*string);
   *string = src ? strdup(src) : NULL;
}

bool vec_size(void **target, size_t elem_size, int count)
{
   const int seg = 4;

   if (--count % seg == 0)
   {
      void *resized = realloc(*target, elem_size * (count + seg));
      if (!resized)
         return false;
      *target = resized;
   }

   return true;
}

bool is_decimal(const char *str)
{
   float v;

   v = 0.0f;
   sscanf(str, "%f", &v);
   return (v && v != (int)v);
}

int get_int(const char *str)
{
   int res;

   res = 0;

   if (str[0] == '#')
      ++str;

   if (str[0] == '$')
   {
      unsigned hex;

      ++str;
      sscanf(str, "%x", &hex);
      res = (int)hex;
   }
   else
   {
      sscanf(str, "%i", &res);
   }

   return res;
}

float get_dec(const char *str)
{
   float res = 0.0f;
   sscanf(str, "%f", &res);

   return res;
}

video_layout_color_t make_color(void)
{
   video_layout_color_t color;
   color.r = 0.0f;
   color.g = 0.0f;
   color.b = 0.0f;
   color.a = 0.0f;
   return color;
}

video_layout_color_t make_color_white(void)
{
   video_layout_color_t color;
   color.r = 1.0f;
   color.g = 1.0f;
   color.b = 1.0f;
   color.a = 1.0f;
   return color;
}

video_layout_color_t make_color_v(float v)
{
   video_layout_color_t color;
   color.r = v;
   color.g = v;
   color.b = v;
   color.a = 1.0f;
   return color;
}

video_layout_color_t make_color_rgb(float r, float g, float b)
{
   video_layout_color_t color;
   color.r = r;
   color.g = g;
   color.b = b;
   color.a = 1.0f;
   return color;
}

video_layout_color_t make_color_rgba(float r, float g, float b, float a)
{
   video_layout_color_t color;
   color.r = r;
   color.g = g;
   color.b = b;
   color.a = a;
   return color;
}

void color_mod(video_layout_color_t *dst, const video_layout_color_t *src)
{
   dst->r *= src->r;
   dst->g *= src->g;
   dst->b *= src->b;
   dst->a *= src->a;
}

video_layout_bounds_t make_bounds(void)
{
   video_layout_bounds_t bounds;
   bounds.x = 0.0f;
   bounds.y = 0.0f;
   bounds.w = 0.0f;
   bounds.h = 0.0f;
   return bounds;
}

video_layout_bounds_t make_bounds_unit(void)
{
   video_layout_bounds_t bounds;
   bounds.x = 0.0f;
   bounds.y = 0.0f;
   bounds.w = 1.0f;
   bounds.h = 1.0f;
   return bounds;
}

video_layout_bounds_t bounds_union(
      const video_layout_bounds_t *a,
      const video_layout_bounds_t *b)
{
   video_layout_bounds_t bounds;

   if (!bounds_valid(a))
      return *b;
   if (!bounds_valid(b))
      return *a;

   bounds.x = MIN(a->x, b->x);
   bounds.y = MIN(a->y, b->y);
   bounds.w = MAX(a->x + a->w, b->x + b->w) - bounds.x;
   bounds.h = MAX(a->y + a->h, b->y + b->h) - bounds.y;

   return bounds;
}

void bounds_scale(
      video_layout_bounds_t *dst,
      const video_layout_bounds_t *dim)
{
   dst->x *= dim->w;
   dst->y *= dim->h;
   dst->w *= dim->w;
   dst->h *= dim->h;
}

bool bounds_valid(const video_layout_bounds_t *bounds)
{
   return (bounds->w > 0 && bounds->h > 0);
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (screensaver_snow_test.c).
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

/* The snow screensaver must stay on screen however long it runs.
 *
 * Flakes drift left and fall down: the drift and fall terms are signed
 * random steps, and a flake re-enters from the right or the top once it
 * leaves past the left or bottom edge.  This includes menu_screensaver.c
 * whole, stubs the font and draw calls, runs the snow effect for an
 * hour of 60 Hz frames at two sizes and requires at least half the
 * flakes on screen at every frame, with the average drift leftward. */

#include <stdio.h>
#include <stdarg.h>

#include "../../../menu/menu_screensaver.c"

/* Stubs for what the effect touches outside this file. */
static font_data_t stub_font;
static video_driver_state_t stub_video_st;

void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
size_t fill_pathname_join(char *s, const char *dir,
      const char *path, size_t len)
{
   (void)dir; (void)path;
   if (len)
      *s = '\0';
   return 0;
}
size_t strlcpy_retro__(char *s, const char *src, size_t len)
{
   size_t n = 0;
   while (src[n])
      n++;
   if (len)
   {
      size_t c = n < len - 1 ? n : len - 1;
      memcpy(s, src, c);
      s[c] = '\0';
   }
   return n;
}
bool path_is_valid(const char *path) { (void)path; return false; }
font_data_t *gfx_display_font_file(gfx_display_t *p_disp,
      char *fontpath, float font_size, bool is_threaded)
{
   (void)p_disp; (void)fontpath; (void)font_size; (void)is_threaded;
   return &stub_font;
}
void font_driver_free(font_data_t *font) { (void)font; }
void font_driver_free_deferred(font_data_t *font) { (void)font; }
void font_driver_bind_block(void *font_data, void *block)
{ (void)font_data; (void)block; }
void video_coord_array_free(video_coord_array_t *ca) { (void)ca; }
video_driver_state_t *video_state_get_ptr(void) { return &stub_video_st; }
void gfx_display_draw_quad(gfx_display_t *p_disp, void *data,
      unsigned video_dims, int x, int y, unsigned dims,
      unsigned ref_dims, float *color, uintptr_t *texture)
{
   (void)p_disp; (void)data; (void)video_dims; (void)x; (void)y;
   (void)dims; (void)ref_dims; (void)color; (void)texture;
}
void gfx_display_draw_text(const font_data_t *font, const char *text,
      float x, float y, unsigned dims, uint32_t color,
      enum text_alignment text_align, float scale_factor,
      bool shadows_enable, float shadow_offset, bool draw_outside)
{
   (void)font; (void)text; (void)x; (void)y; (void)dims; (void)color;
   (void)text_align; (void)scale_factor; (void)shadows_enable;
   (void)shadow_offset; (void)draw_outside;
}

#define FRAMES (60 * 60 * 60)

static int run(unsigned width, unsigned height, uint32_t seed)
{
   gfx_display_t disp;
   gfx_animation_t anim;
   menu_screensaver_t *ss = menu_screensaver_init();
   unsigned dims          = VIDEO_SCALE_PACK(width, height);
   double drift           = 0.0;
   unsigned long frame;
   unsigned visible;
   size_t i;

   memset(&disp, 0, sizeof(disp));
   memset(&anim, 0, sizeof(anim));
   anim.delta_time   = MENU_SS_EFFECT_PERIOD;
   menu_ss_rng_state = seed;

   if (!ss)
   {
      printf("[FAIL] %ux%u: init\n", width, height);
      return 1;
   }

   for (frame = 0; frame < FRAMES; frame++)
   {
      menu_screensaver_iterate(ss, &disp, &anim,
            MENU_SCREENSAVER_SNOW, 1.0f, 0xFFFFFF, dims, NULL);

      if (!ss->particles)
      {
         printf("[FAIL] %ux%u: no particles\n", width, height);
         menu_screensaver_free(ss);
         return 1;
      }

      for (i = 0, visible = 0; i < MENU_SS_NUM_PARTICLES; i++)
      {
         const menu_ss_particle_t *p = &ss->particles[i];
         drift += p->a;
         if (     p->x >= 0.0f && p->x <= (float)width
               && p->y >= 0.0f && p->y <= (float)height)
            visible++;
      }

      if (visible < MENU_SS_NUM_PARTICLES / 2)
      {
         printf("[FAIL] %ux%u: %u of %u flakes on screen after %lu frames\n",
               width, height, visible, (unsigned)MENU_SS_NUM_PARTICLES,
               frame + 1);
         menu_screensaver_free(ss);
         return 1;
      }
   }

   drift /= (double)FRAMES * MENU_SS_NUM_PARTICLES;
   menu_screensaver_free(ss);

   if (drift >= 0.0)
   {
      printf("[FAIL] %ux%u: average drift %.3f, expected leftward\n",
            width, height, drift);
      return 1;
   }
   printf("[ok]   %ux%u: one hour on screen, average drift %.3f\n",
         width, height, drift);
   return 0;
}

int main(void)
{
   int failures = 0;
   failures += run(1920, 1080, 0x12345678u);
   failures += run(320,  240,  0x9e3779b9u);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}

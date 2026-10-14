/* The cases the D3D9 drivers' preset setup must decide alike: the last
 * pass of a preset gets a target of its own, drawn to the backbuffer by
 * a stock pass appended after it, only when drawing it straight there
 * would give a different image - a scale other than the viewport at 1.0,
 * or a floating-point target.  Included by each test after the driver
 * source it includes; the test passes in the driver's setup function. */

#ifndef D3D9_LAST_PASS_CASES_H
#define D3D9_LAST_PASS_CASES_H

#include <stdio.h>
#include <string.h>

/* The scripted preset the loader hands out. */
static struct video_shader preset;

bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader)
{
   (void)path;
   memcpy(shader, &preset, sizeof(*shader));
   return true;
}

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

static void scale_set(struct gfx_fbo_scale *s, enum gfx_scale_type type,
      float fx, float fy)
{
   memset(s, 0, sizeof(*s));
   s->flags   = FBO_SCALE_FLAG_VALID;
   s->type_x  = type;
   s->type_y  = type;
   s->scale_x = fx;
   s->scale_y = fy;
}

/* The pass count @init leaves for the scripted preset, which also must
 * end on a viewport pass. */
static unsigned run_init(bool (*init)(d3d9_video_t*, const char*),
      d3d9_video_t *d3d)
{
   struct video_shader_pass *last;
   if (!init(d3d, "preset"))
      return 0;
   last = &d3d->shader.pass[d3d->shader.passes - 1];
   if (     last->fbo.type_x  != RARCH_SCALE_VIEWPORT
         || last->fbo.type_y  != RARCH_SCALE_VIEWPORT
         || last->fbo.scale_x != 1.0f
         || last->fbo.scale_y != 1.0f)
      return 0;
   return d3d->shader.passes;
}

static int run_cases(bool (*init)(d3d9_video_t*, const char*))
{
   static d3d9_video_t d3d;

   printf("two passes:\n");
   memset(&preset, 0, sizeof(preset));
   preset.passes = 2;
   scale_set(&preset.pass[0].fbo, RARCH_SCALE_INPUT, 2.0f, 2.0f);

   memset(&preset.pass[1].fbo, 0, sizeof(preset.pass[1].fbo));
   CHECK(run_init(init, &d3d) == 2, "no scale on the last pass: drawn straight");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_VIEWPORT, 1.0f, 1.0f);
   CHECK(run_init(init, &d3d) == 2, "viewport at 1.0: drawn straight");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_VIEWPORT, 2.0f, 1.0f);
   CHECK(run_init(init, &d3d) == 3, "viewport at 2.0 across: a stock pass after it");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_VIEWPORT, 1.0f, 0.5f);
   CHECK(run_init(init, &d3d) == 3, "viewport at 0.5 down: a stock pass after it");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_VIEWPORT, 1.0f, 1.0f);
   preset.pass[1].fbo.type_y = RARCH_SCALE_ABSOLUTE;
   preset.pass[1].fbo.abs_y  = 240;
   CHECK(run_init(init, &d3d) == 3, "absolute down: a stock pass after it");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_INPUT, 1.0f, 1.0f);
   CHECK(run_init(init, &d3d) == 3, "source at 1.0: a stock pass after it");

   scale_set(&preset.pass[1].fbo, RARCH_SCALE_VIEWPORT, 1.0f, 1.0f);
   preset.pass[1].fbo.flags |= FBO_SCALE_FLAG_FP_FBO;
   CHECK(run_init(init, &d3d) == 3, "float_framebuffer: a stock pass after it");

   printf("one pass:\n");
   preset.passes = 1;

   memset(&preset.pass[0].fbo, 0, sizeof(preset.pass[0].fbo));
   CHECK(run_init(init, &d3d) == 1, "no scale: drawn straight");

   scale_set(&preset.pass[0].fbo, RARCH_SCALE_VIEWPORT, 1.0f, 1.0f);
   CHECK(run_init(init, &d3d) == 1, "viewport at 1.0: drawn straight");

   scale_set(&preset.pass[0].fbo, RARCH_SCALE_INPUT, 2.0f, 2.0f);
   CHECK(run_init(init, &d3d) == 2, "source at 2.0: a stock pass after it");

   printf("a full preset:\n");
   memset(&preset, 0, sizeof(preset));
   preset.passes = GFX_MAX_SHADERS;
   scale_set(&preset.pass[GFX_MAX_SHADERS - 1].fbo, RARCH_SCALE_INPUT,
         2.0f, 2.0f);
   CHECK(run_init(init, &d3d) == GFX_MAX_SHADERS,
         "no room for a stock pass: the last pass draws to the viewport");

   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}

#endif

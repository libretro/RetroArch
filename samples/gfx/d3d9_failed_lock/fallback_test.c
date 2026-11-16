/* A D3D9 menu effect that would not compile is drawn as the background
 * is: the driver's draw_pipeline() leaves the draw as it was handed -
 * the background's coordinates, its position, no effect geometry - and
 * draw() takes the plain quad path, with the stock shader, in the
 * colour the menu gives the effect. Before, it was drawn by the stock
 * shader from the effect's own white vertices, and covered the menu in
 * white. Built once per backend from the driver's own code (run.py
 * cuts it out of d3d9hlsl.c and d3d9cg.c). */

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#define VIDEO_POS_PACK(x, y) ((unsigned)(x) | ((unsigned)(y) << 16))

struct video_coords { const float *vertex; unsigned vertices; };
typedef struct { int unused; } gfx_display_t;
typedef struct
{
   const void *effect;
   struct video_coords *coords;
   unsigned pos;
   const void *matrix_data;
} gfx_display_ctx_draw_t;

static struct video_coords effect_coords;
static unsigned effect_coords_asked;

static struct video_coords *gfx_display_effect_coords(gfx_display_t *p)
{
   (void)p;
   effect_coords_asked++;
   return &effect_coords;
}

#if defined(TEST_HLSL)
struct shader_pass { void *vprg; void *fprg; };
typedef struct { int unused; } hlsl_pass_data_t;
typedef struct { void *renderchain_data; } d3d9_video_t;
#include "fallback_types.inc"
#include "fallback_helper.inc"
typedef struct d3d9_hlsl_effect effect_t;
#define DRAWABLE(d3d, fx) d3d9_hlsl_effect_drawable(fx)
static void pipeline_head(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp, void *data)
#include "fallback_pipeline.inc"
#elif defined(TEST_CG)
typedef struct gfx_display_effect_desc gfx_display_effect_desc_t;
typedef void *CGprogram;
typedef void *CGparameter;
#include "fallback_types.inc"
typedef struct { struct d3d9_cg_effect_prog
   {
      CGprogram vprg;
      CGprogram fprg;
      CGparameter mvp, vtime, ftime, ribbon, voutput, foutput, alpha;
   } effects[16]; } cg_renderchain_t;
typedef struct { void *renderchain_data; } d3d9_video_t;
static struct d3d9_cg_effect d3d9_cg_effects[16];
#include "fallback_helper.inc"
typedef struct d3d9_cg_effect effect_t;
#define DRAWABLE(d3d, fx) d3d9_cg_effect_drawable(d3d, fx)
static void pipeline_head(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp, void *data)
#include "fallback_pipeline.inc"
#endif

/* The value draw()'s switch takes for @draw: 1 the effect's own path,
 * 0 the plain quad path */
static int draw_path(gfx_display_ctx_draw_t *draw, d3d9_video_t *d3d)
{
   (void)d3d;
   return
#include "fallback_switch.inc"
      ;
}

int main(void)
{
   static int vprg, fprg;
   struct video_coords bg;
   gfx_display_t disp;
   gfx_display_ctx_draw_t draw;
#if defined(TEST_HLSL)
   static effect_t effects[2];
   d3d9_video_t d3d   = { NULL };
   effects[0].shader.vprg = &vprg;
   effects[0].shader.fprg = &fprg;
#else
   static cg_renderchain_t chain;
   effect_t *effects  = d3d9_cg_effects;
   d3d9_video_t d3d   = { &chain };
   chain.effects[0].vprg = &vprg;
   chain.effects[0].fprg = &fprg;
#endif

   /* One that compiled: drawn from its own geometry, by its own path */
   bg.vertex   = NULL;
   bg.vertices = 4;
   draw.effect = &effects[0];
   draw.coords = &bg;
   draw.pos    = VIDEO_POS_PACK(5, 6);
   assert(DRAWABLE(&d3d, &effects[0]));
   pipeline_head(&draw, &disp, &d3d);
   assert(effect_coords_asked == 1);
   assert(draw_path(&draw, &d3d) == 1);

   /* One that did not: the draw is left as the background's, and
    * goes the plain way */
   draw.effect = &effects[1];
   draw.coords = &bg;
   draw.pos    = VIDEO_POS_PACK(5, 6);
   assert(!DRAWABLE(&d3d, &effects[1]));
   pipeline_head(&draw, &disp, &d3d);
   assert(effect_coords_asked == 1);
   assert(draw.coords == &bg && draw.pos == VIDEO_POS_PACK(5, 6));
   assert(draw_path(&draw, &d3d) == 0);

   /* No effect at all: the plain way */
   draw.effect = NULL;
   assert(draw_path(&draw, &d3d) == 0);
   return 0;
}

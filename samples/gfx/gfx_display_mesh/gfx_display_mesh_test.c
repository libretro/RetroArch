/* The mesh facility in gfx/gfx_display.c, against a display driver that
 * records what it is handed.
 *
 *  create      a mesh is refused for bad indices, a triangle count
 *              that is not a whole number of triangles, or no
 *              vertices; GFX_MESH_FLAG_POSITIONS keeps each vertex's
 *              x and y as plain pairs, and nothing else does.
 *  cpu         a plain program on a driver that does not draw meshes:
 *              positions through the MVP into 0..1 display space,
 *              texture coordinates and tinted colours, triangles as
 *              five strip vertices each, strips as they are, triangles
 *              behind the eye dropped, the white texture for COLORED.
 *  effects     each effect program reaches the driver's pipeline code
 *              under its pipeline id, over the background's state,
 *              with the effect coordinates naming the mesh it is
 *              drawn over; a mesh with no positions draws no effect.
 *  quads-only  a driver that takes no strips is handed nothing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gfx/gfx_display.h"
#include "gfx/video_driver.h"

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ---- what gfx_display.c reaches outside itself -------------------- */

void video_coord_array_free(video_coord_array_t *ca) { (void)ca; }
void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }

/* ---- a recording display driver ------------------------------------ */

static float    rec_xy[4096], rec_uv[4096], rec_col[8192];
static unsigned rec_vertices, rec_draws, rec_pipeline_calls, rec_pipeline_id;
static uintptr_t rec_texture;

static void rec_draw(gfx_display_ctx_draw_t *d, void *data, unsigned vd)
{
   (void)data; (void)vd;
   rec_draws++;
   rec_texture  = d->texture;
   rec_vertices = d->coords ? d->coords->vertices : 0;
   if (d->coords && d->coords->vertex && rec_vertices <= 2048)
      memcpy(rec_xy, d->coords->vertex, rec_vertices * 2 * sizeof(float));
   if (d->coords && d->coords->tex_coord && rec_vertices <= 2048)
      memcpy(rec_uv, d->coords->tex_coord, rec_vertices * 2 * sizeof(float));
   if (d->coords && d->coords->color && rec_vertices <= 2048)
      memcpy(rec_col, d->coords->color, rec_vertices * 4 * sizeof(float));
}
static const float *rec_effect_vertex;
static unsigned     rec_effect_vertices;
static void rec_pipeline(gfx_display_ctx_draw_t *d, gfx_display_t *p,
      void *data, unsigned vd)
{
   struct video_coords *ec = gfx_display_effect_coords(p);
   (void)data; (void)vd;
   rec_pipeline_calls++;
   rec_pipeline_id     = d->pipeline_id;
   rec_effect_vertex   = ec ? ec->vertex : NULL;
   rec_effect_vertices = ec ? ec->vertices : 0;
}
static const float def_vert[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
static const float def_tex[8]  = { 0, 1, 1, 1, 0, 0, 1, 0 };
static const float *rec_def_vert(void) { return def_vert; }
static const float *rec_def_tex(void)  { return def_tex; }

static gfx_display_ctx_driver_t rec_ctx;

static void reset(void)
{
   rec_vertices = rec_draws = rec_pipeline_calls = rec_pipeline_id = 0;
   rec_texture  = 0;
}

static gfx_display_mesh_t *make(const gfx_display_mesh_vertex_t *v, unsigned n,
      const uint16_t *idx, unsigned ni, enum gfx_display_mesh_topology t,
      unsigned flags)
{
   gfx_display_mesh_desc_t d;
   d.vertices = v; d.vertex_count = n; d.indices = idx; d.index_count = ni;
   d.topology = t; d.flags = flags;
   return gfx_display_mesh_create(&d);
}

static void vtx(gfx_display_mesh_vertex_t *v, float x, float y, float z,
      uint16_t u, uint16_t w, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
   v->x = x; v->y = y; v->z = z; v->u = u; v->v = w;
   v->rgba[0] = r; v->rgba[1] = g; v->rgba[2] = b; v->rgba[3] = a;
}

int main(void)
{
   gfx_display_t *p_disp = disp_get_ptr();
   gfx_display_mesh_vertex_t v[8];
   gfx_display_mesh_t *mesh;
   gfx_display_mesh_draw_t md;
   float white[16];
   unsigned i;
   const unsigned vd = VIDEO_SCALE_PACK(640, 480);

   for (i = 0; i < 16; i++)
      white[i] = 1.0f;
   memset(&rec_ctx, 0, sizeof(rec_ctx));
   rec_ctx.draw                   = rec_draw;
   rec_ctx.draw_pipeline          = rec_pipeline;
   rec_ctx.get_default_vertices   = rec_def_vert;
   rec_ctx.get_default_tex_coords = rec_def_tex;
   rec_ctx.handles_vertex_strip   = true;
   p_disp->dispctx                = &rec_ctx;

   /* create */
   vtx(&v[0], -1, -1, 0, 0, 0, 255, 0, 0, 255);
   vtx(&v[1],  1, -1, 0, 65535, 0, 0, 255, 0, 255);
   vtx(&v[2], -1,  1, 0, 0, 65535, 0, 0, 255, 255);
   vtx(&v[3],  1,  1, 0, 65535, 65535, 255, 255, 255, 128);
   {
      static const uint16_t bad[3]  = { 0, 1, 4 };
      static const uint16_t good[6] = { 0, 1, 2, 2, 1, 3 };
      CHECK(!make(v, 4, bad, 3, GFX_MESH_TRIANGLES, 0), "an index past the vertices is refused");
      CHECK(!make(v, 4, good, 5, GFX_MESH_TRIANGLES, 0), "a part triangle is refused");
      CHECK(!make(v, 0, NULL, 0, GFX_MESH_TRIANGLE_STRIP, 0), "no vertices is refused");
      mesh = make(v, 4, NULL, 0, GFX_MESH_TRIANGLE_STRIP, GFX_MESH_FLAG_POSITIONS);
      CHECK(mesh && mesh->positions
            && mesh->positions[0] == -1 && mesh->positions[1] == -1
            && mesh->positions[6] ==  1 && mesh->positions[7] ==  1,
            "GFX_MESH_FLAG_POSITIONS keeps x and y as pairs");
      gfx_display_mesh_free(mesh);
      mesh = make(v, 4, good, 6, GFX_MESH_TRIANGLES, 0);
      CHECK(mesh && !mesh->positions, "and nothing else does");
      CHECK(gfx_display_mesh_fullscreen()->positions != NULL,
            "the full-screen quad has its positions");
   }

   /* cpu: two indexed triangles, identity transform */
   reset();
   md.mvp = NULL; md.color = white; md.texture = 7; md.program = GFX_MESH_PROGRAM_TEXTURED;
   gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
   CHECK(rec_draws == 1 && rec_vertices == 10, "two triangles are ten strip vertices, one draw");
   CHECK(rec_texture == 7, "TEXTURED draws with its texture");
   CHECK(   NEAR(rec_xy[0], 0.0f) && NEAR(rec_xy[1], 0.0f)
         && NEAR(rec_xy[2], 0.0f) && NEAR(rec_xy[3], 0.0f)
         && NEAR(rec_xy[4], 1.0f) && NEAR(rec_xy[5], 0.0f)
         && NEAR(rec_xy[6], 0.0f) && NEAR(rec_xy[7], 1.0f)
         && NEAR(rec_xy[8], 0.0f) && NEAR(rec_xy[9], 1.0f),
         "clip space -1..1 lands on 0..1, the first triangle a a b c c");
   CHECK(NEAR(rec_xy[18], 1.0f) && NEAR(rec_xy[19], 1.0f), "the last vertex is the far corner");
   CHECK(NEAR(rec_uv[18], 1.0f) && NEAR(rec_uv[19], 1.0f), "texture coordinates in 1/65535ths");
   CHECK(NEAR(rec_col[0], 1.0f) && NEAR(rec_col[1], 0.0f) && NEAR(rec_col[39], 128.0f / 255.0f),
         "colours from the vertices");

   /* tint, COLORED */
   reset();
   {
      float tint[16];
      for (i = 0; i < 16; i++) tint[i] = 0.5f;
      md.color = tint; md.program = GFX_MESH_PROGRAM_COLORED; md.texture = 7;
      gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
      CHECK(NEAR(rec_col[0], 0.5f) && NEAR(rec_col[3], 0.5f), "the tint multiplies the vertex colour");
      CHECK(rec_texture != 7, "COLORED draws with the white texture, not the one handed in");
   }

   /* perspective: a point at z = -2 through a projection with w = -z */
   reset();
   {
      /* x and y scaled by 1, w = -z: a point (1, 1, -2) lands at 0.5 in ndc */
      static const float proj[16] = { 1,0,0,0,  0,1,0,0,  0,0,1,-1,  0,0,0,0 };
      gfx_display_mesh_vertex_t t[3];
      gfx_display_mesh_t *tri;
      vtx(&t[0], 0, 0, -2, 0, 0, 255, 255, 255, 255);
      vtx(&t[1], 1, 0, -2, 0, 0, 255, 255, 255, 255);
      vtx(&t[2], 1, 1, -2, 0, 0, 255, 255, 255, 255);
      tri = make(t, 3, NULL, 0, GFX_MESH_TRIANGLES, 0);
      md.mvp = proj; md.color = white; md.program = GFX_MESH_PROGRAM_COLORED;
      gfx_display_mesh_draw(p_disp, NULL, vd, tri, &md);
      CHECK(rec_vertices == 5 && NEAR(rec_xy[8], 0.75f) && NEAR(rec_xy[9], 0.75f),
            "the perspective divide: (1,1,-2) lands at 0.75");
      /* the same triangle in front of the eye: w = -2 */
      reset();
      vtx(&t[0], 0, 0, 2, 0, 0, 255, 255, 255, 255);
      gfx_display_mesh_free(tri);
      tri = make(t, 3, NULL, 0, GFX_MESH_TRIANGLES, 0);
      gfx_display_mesh_draw(p_disp, NULL, vd, tri, &md);
      CHECK(rec_draws == 0, "a triangle reaching behind the eye is not drawn");
      gfx_display_mesh_free(tri);
      md.mvp = NULL;
   }

   /* strips as they are */
   reset();
   {
      gfx_display_mesh_t *strip = make(v, 4, NULL, 0, GFX_MESH_TRIANGLE_STRIP, 0);
      md.program = GFX_MESH_PROGRAM_TEXTURED; md.texture = 3;
      gfx_display_mesh_draw(p_disp, NULL, vd, strip, &md);
      CHECK(rec_vertices == 4 && NEAR(rec_xy[6], 1.0f) && NEAR(rec_xy[7], 1.0f), "a strip goes as it is");
      gfx_display_mesh_free(strip);
   }

   /* effects */
   {
      static const struct { enum gfx_display_mesh_program p; unsigned id; } map[] = {
         { GFX_MESH_PROGRAM_BLEND,         VIDEO_SHADER_STOCK_BLEND },
         { GFX_MESH_PROGRAM_RIBBON,        VIDEO_SHADER_MENU },
         { GFX_MESH_PROGRAM_RIBBON_SIMPLE, VIDEO_SHADER_MENU_2 },
         { GFX_MESH_PROGRAM_SNOW_SIMPLE,   VIDEO_SHADER_MENU_3 },
         { GFX_MESH_PROGRAM_SNOW,          VIDEO_SHADER_MENU_4 },
         { GFX_MESH_PROGRAM_BOKEH,         VIDEO_SHADER_MENU_5 },
         { GFX_MESH_PROGRAM_SNOWFLAKE,     VIDEO_SHADER_MENU_6 } };
      for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      {
         reset();
         md.program = map[i].p; md.color = white; md.texture = 0;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(rec_pipeline_calls == 1 && rec_pipeline_id == map[i].id && rec_draws == 1,
               "an effect reaches the pipeline under its id, then draws");
         CHECK(rec_effect_vertex == gfx_display_mesh_fullscreen()->positions
               && rec_effect_vertices == 4,
               "the effect coordinates are the mesh it is drawn over");
      }
      CHECK(gfx_display_effect_coords(p_disp) == NULL,
            "and there are none outside the draw");
      reset();
      md.program = GFX_MESH_PROGRAM_RIBBON;
      gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
      CHECK(rec_pipeline_calls == 0 && rec_draws == 0,
            "a mesh with no positions draws no effect");
      CHECK(NEAR(white[3], 1.0f), "the effect leaves the colour's alpha as it was");
   }

   /* quads-only */
   reset();
   rec_ctx.handles_vertex_strip = false;
   md.program = GFX_MESH_PROGRAM_TEXTURED;
   gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
   CHECK(rec_draws == 0, "a driver that takes no strips is handed nothing");

   gfx_display_mesh_free(mesh);
   p_disp->dispctx = NULL;
   gfx_display_free();

   if (fails)
   {
      printf("gfx_display_mesh_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("gfx_display_mesh_test: all checks passed\n");
   return 0;
}

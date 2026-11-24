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
 *  effects     an effect installed twice is one effect; the driver
 *              makes every installed effect's program at the first
 *              effect draw, once, and again after it goes; each draw
 *              reaches the driver's pipeline code with its program,
 *              over the background's state, with the effect
 *              coordinates naming the mesh it is drawn over; a mesh
 *              with no positions, or a driver that makes no programs,
 *              draws no effect.
 *  quads-only  a driver that takes no strips is handed nothing.
 *  stretch     with an output size set, quads and draws laid out at
 *              another width reach the driver across the output's.
 *  blur plans  the backdrop blur's sizes - a quarter of the rect, at
 *              most 480 x 270, from a copy at twice that - and its six
 *              passes, one axis each, each reading the last and twice
 *              as wide as the round before, ending in image 0.
 *  blur        a backdrop blur reaches a driver that has one after
 *              the quads gathered before it are drawn, with its alpha
 *              and dim held to 0..1 and its rect stretched like a
 *              quad's; with no hook, no alpha, a rect under 8 pixels
 *              or a driver that cannot, nothing is blurred and the
 *              caller is told. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <features/features_cpu.h>

#include "gfx/gfx_display.h"
#include "gfx/common/menu_ribbon.h"
#include "gfx/common/menu_bloom.h"
#include "gfx/video_driver.h"
#include "configuration.h"

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)
#define TEST_NEAR(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ---- what gfx_display.c reaches outside itself -------------------- */

void video_coord_array_free(video_coord_array_t *ca) { (void)ca; }
void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }

bool win32_message_modal_active(void) { return false; }
bool video_context_driver_get_metrics(gfx_ctx_metrics_t *m) { (void)m; return false; }
void *video_driver_get_ptr(void) { return NULL; }
video_driver_state_t *video_state_get_ptr(void) { return NULL; }
bool input_osk_native_active(void) { return false; }
settings_t *config_get_ptr(void) { return NULL; }
bool video_driver_has_windowed(void) { return false; }
const char *video_driver_get_ident(void) { return "test"; }
font_data_t *font_driver_init_first(void *data, const char *path,
      float size, bool hint, bool threaded, const font_renderer_t *backend)
{
   (void)data; (void)path; (void)size; (void)hint; (void)threaded; (void)backend;
   return NULL;
}

/* The effects animate by the frame's time they are handed; the clock
 * itself is not theirs to read, and a read here is counted. */
static retro_time_t test_time = 1000000;
static unsigned test_clock_reads;
retro_time_t cpu_features_get_time_usec(void)
{
   test_clock_reads++;
   return 0;
}

/* ---- a recording display driver ------------------------------------ */

static float    rec_xy[4096], rec_uv[4096], rec_col[8192];
static unsigned rec_vertices, rec_draws, rec_pipeline_calls;
static void    *rec_pipeline_effect;
static unsigned rec_pos, rec_dims, rec_vd;
static uintptr_t rec_texture;

static void rec_draw(gfx_display_ctx_draw_t *d, void *data, unsigned vd)
{
   (void)data;
   rec_draws++;
   rec_pos      = d->pos;
   rec_dims     = d->dims;
   rec_vd       = vd;
   rec_texture  = d->texture;
   rec_vertices = d->coords ? d->coords->vertices : 0;
   if (d->coords && d->coords->vertex && rec_vertices <= 2048)
      memcpy(rec_xy, d->coords->vertex, rec_vertices * 2 * sizeof(float));
   if (d->coords && d->coords->tex_coord && rec_vertices <= 2048)
      memcpy(rec_uv, d->coords->tex_coord, rec_vertices * 2 * sizeof(float));
   if (d->coords && d->coords->color && rec_vertices <= 2048)
      memcpy(rec_col, d->coords->color, rec_vertices * 4 * sizeof(float));
}
static bool rec_media_shader(void *data, uintptr_t texture, unsigned source,
      unsigned video, unsigned pos, unsigned dims)
{
   CHECK(rec_draws == 1, "menu batches draw before a viewer shader");
   CHECK(texture == 123 && source == VIDEO_SCALE_PACK(160, 120),
         "the viewer lends its original texture and dimensions");
   rec_vd = video;
   rec_pos = pos;
   rec_dims = dims;
   return true;
}

static const float *rec_effect_vertex;
static unsigned     rec_effect_vertices;
static float        rec_effect_time;
static void rec_pipeline(gfx_display_ctx_draw_t *d, gfx_display_t *p,
      void *data, unsigned vd)
{
   struct video_coords *ec = gfx_display_effect_coords(p);
   (void)data; (void)vd;
   rec_pipeline_calls++;
   rec_pipeline_effect = d->effect;
   rec_effect_vertex   = ec ? ec->vertex : NULL;
   rec_effect_vertices = ec ? ec->vertices : 0;
   rec_effect_time     = p->effect_time;
}

static unsigned rec_bloom_calls, rec_bloom_deferred, rec_bloom_composites;
static unsigned rec_bloom_draws_at_composite;
static bool rec_bloom_check_time = true;
static unsigned rec_bloom_begin(void *data, unsigned vd, bool defer)
{
   (void)data;
   rec_bloom_calls++;
   if (defer)
      rec_bloom_deferred++;
   test_time += 50000;
   return vd;
}

static void rec_bloom_end(void *data, unsigned vd, bool defer)
{
   (void)data; (void)vd;
   if (!defer && rec_bloom_check_time)
      CHECK(TEST_NEAR(rec_effect_time, 0.6f),
            "bloom capture uses the frame's animation time");
   test_time += 50000;
}

/* The program the driver makes of an effect: its description, counted */
static unsigned rec_installs;
static void *rec_effect_install(void *data,
      const gfx_display_effect_desc_t *desc)
{
   (void)data;
   rec_installs++;
   return (void*)desc;
}

static void rec_bloom_composite(void *data, unsigned vd)
{
   (void)data; (void)vd;
   rec_bloom_composites++;
   rec_bloom_draws_at_composite = rec_draws;
}
/* The backdrop blur: what it was asked, and what was drawn by then */
static unsigned rec_blurs, rec_blur_vd, rec_blur_dims, rec_blur_draws;
static int rec_blur_x, rec_blur_y;
static float rec_blur_alpha, rec_blur_dim;
static bool rec_blur_result;
static bool rec_blur_rect(void *data, unsigned vd, int x, int y,
      unsigned dims, float alpha, float dim)
{
   (void)data;
   rec_blurs++;
   rec_blur_vd    = vd;
   rec_blur_x     = x;
   rec_blur_y     = y;
   rec_blur_dims  = dims;
   rec_blur_alpha = alpha;
   rec_blur_dim   = dim;
   rec_blur_draws = rec_draws;
   return rec_blur_result;
}

static const float def_vert[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
static const float def_tex[8]  = { 0, 1, 1, 1, 0, 0, 1, 0 };
static const float *rec_def_vert(void) { return def_vert; }
static const float *rec_def_tex(void)  { return def_tex; }

static gfx_display_ctx_driver_t rec_ctx;

static void reset(void)
{
   rec_vertices = rec_draws = rec_pipeline_calls = 0;
   rec_pipeline_effect = NULL;
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

static void ribbon_motion(void)
{
   menu_ribbon_t *a = (menu_ribbon_t*)calloc(1, sizeof(*a));
   menu_ribbon_t *b = (menu_ribbon_t*)calloc(1, sizeof(*b));
   unsigned i;
   float time, lo = 100.0f, hi = 0.0f;
   CHECK(a && b, "ribbon state allocation");
   if (!a || !b)
   {
      free(a);
      free(b);
      return;
   }
   for (i = 0; i <= 1200; i++)
      menu_ribbon_update(a, i / 30.0, 1280, 720, 1.0f);
   for (i = 0; i <= 4800; i++)
      menu_ribbon_update(b, i / 120.0, 1280, 720, 1.0f);
   for (i = 0; i < MENU_RIBBON_UNIFORMS * 4; i++)
      CHECK(fabsf(a->data[i / 4][i % 4] - b->data[i / 4][i % 4]) < 0.001f,
            "ribbon and dust motion are independent of refresh rate");
   for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
   {
      float light = a->data[1 + i][3];
      if (light < lo)
         lo = light;
      if (light > hi)
         hi = light;
      CHECK(a->particles[i].age >= 0.0f
            && a->particles[i].age < a->particles[i].life,
            "particles renew their own lifetimes");
      CHECK(fabsf(a->particles[i].y) < 0.9f,
            "dust remains near the ribbon");
   }
   CHECK(hi - lo > 0.2f, "particle brightness is not synchronized");
   {
      /* Above 60 Hz every frame moves on by the time it covers */
      unsigned still = 0, uneven = 0;
      float prev;
      memset(b, 0, sizeof(*b));
      menu_ribbon_update(b, 0.0, 1280, 720, 1.0f);
      prev = b->data[0][0];
      for (i = 1; i <= 1440; i++)
      {
         float step;
         menu_ribbon_update(b, i / 144.0, 1280, 720, 1.0f);
         step = b->data[0][0] - prev;
         prev = b->data[0][0];
         if (step <= 0.0f)
            still++;
         if (fabsf(step - (float)MENU_RIBBON_SPEED / 144.0f) > 1e-4f)
            uneven++;
      }
      CHECK(still == 0, "the ribbon moves on every frame at 144 Hz");
      CHECK(uneven == 0, "each 144 Hz frame moves the ribbon by its own time");
   }
   time = a->time;
   menu_ribbon_update(a, 3600.0, 1280, 720, 0.4f);
   CHECK(a->time - time <= 0.067f, "resume has a bounded simulation step");
   CHECK(a->data[49][2] == 0.4f, "menu fading reaches both sheet and dust");
   free(a);
   free(b);
}

/* The glow's passes as the GPU runs them: bilinear, clamped reads */
#define BW 24
#define BH 12
static float bloom_read(const float *img, float u, float v)
{
   float x = u * BW - 0.5f, y = v * BH - 0.5f;
   int x0 = (int)floorf(x), y0 = (int)floorf(y);
   float fx = x - x0, fy = y - y0, r = 0.0f;
   int i, j;
   for (j = 0; j < 2; j++)
      for (i = 0; i < 2; i++)
      {
         int xi = x0 + i < 0 ? 0 : (x0 + i >= BW ? BW - 1 : x0 + i);
         int yj = y0 + j < 0 ? 0 : (y0 + j >= BH ? BH - 1 : y0 + j);
         r += img[yj * BW + xi] * (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
      }
   return r;
}

static float bloom_light(const float *img, float u, float v, float t)
{
   float c = bloom_read(img, u, v) - t;
   return c > 0.0f ? c : 0.0f;
}

static void bloom_run(bool floating, const float *capture, float *out)
{
   static float img[2][BW * BH];
   menu_bloom_pass_t plan[MENU_BLOOM_PASSES];
   unsigned n = menu_bloom_plan(plan, VIDEO_SCALE_PACK(BW, BH), floating);
   unsigned i;
   int x, y;
   memcpy(img[0], capture, sizeof(img[0]));
   memset(out, 0, BW * BH * sizeof(float));
   for (i = 0; i < n; i++)
   {
      static float tmp[BW * BH];
      const float *p = plan[i].params;
      for (y = 0; y < BH; y++)
         for (x = 0; x < BW; x++)
         {
            float u = (x + 0.5f) / BW, v = (y + 0.5f) / BH;
            const float *src = img[plan[i].src];
            float c = bloom_light(src, u, v, p[2]);
            if (p[0] != 0.0f || p[1] != 0.0f)
            {
               c *= 0.227027f;
               c += (bloom_light(src, u + p[0] * 1.384615f, v + p[1] * 1.384615f, p[2])
                   + bloom_light(src, u - p[0] * 1.384615f, v - p[1] * 1.384615f, p[2]))
                  * 0.316216f;
               c += (bloom_light(src, u + p[0] * 3.230769f, v + p[1] * 3.230769f, p[2])
                   + bloom_light(src, u - p[0] * 3.230769f, v - p[1] * 3.230769f, p[2]))
                  * 0.0702703f;
            }
            tmp[y * BW + x] = c * p[3];
         }
      for (x = 0; x < BW * BH; x++)
      {
         float *dst = plan[i].dst == MENU_BLOOM_OUTPUT
            ? &out[x] : &img[plan[i].dst][x];
         *dst = plan[i].add ? *dst + tmp[x] : tmp[x];
      }
   }
}

static void bloom_plans(void)
{
   static float capture[BW * BH], two[BW * BH], one[BW * BH];
   menu_bloom_pass_t plan[MENU_BLOOM_PASSES];
   float worst = 0.0f, peak = 0.0f;
   int i;
   for (i = 0; i < BW * BH; i++)
      capture[i] = (float)((i * 7919) % 97) / 40.0f;
   bloom_run(false, capture, two);
   bloom_run(true, capture, one);
   for (i = 0; i < BW * BH; i++)
   {
      float d = fabsf(two[i] - one[i]);
      worst = d > worst ? d : worst;
      peak  = two[i] > peak ? two[i] : peak;
   }
   CHECK(peak > 0.1f && worst < 1e-5f * peak,
         "float images glow the same in five passes as in six");
   CHECK(menu_bloom_plan(plan, VIDEO_SCALE_PACK(BW, BH), true) == 5
         && plan[4].dst == MENU_BLOOM_OUTPUT,
         "and only the last of the five writes the output");
}

/* The backdrop blur's sizes and passes, shared by the drivers */
static void blur_plans(void)
{
   menu_bloom_pass_t plan[MENU_BLUR_PASSES];
   unsigned small = menu_blur_dims(VIDEO_SCALE_PACK(1920, 1080));
   unsigned i, last = 1;
   bool ok = true;
   CHECK(small == VIDEO_SCALE_PACK(480, 270)
         && MENU_BLUR_COPY_DIMS(small) == VIDEO_SCALE_PACK(960, 540),
         "1080p blurs at a quarter, from a copy at half");
   CHECK(menu_blur_dims(VIDEO_SCALE_PACK(3840, 2160))
            == VIDEO_SCALE_PACK(480, 270)
         && menu_blur_dims(VIDEO_SCALE_PACK(3840, 1080))
            == VIDEO_SCALE_PACK(480, 135),
         "larger outputs are held to 480 x 270, their aspect kept");
   CHECK(menu_blur_dims(VIDEO_SCALE_PACK(640, 480))
            == VIDEO_SCALE_PACK(160, 120)
         && menu_blur_dims(VIDEO_SCALE_PACK(9, 2))
            == VIDEO_SCALE_PACK(3, 1),
         "smaller ones at a quarter, never to nothing");
   menu_blur_plan(plan, small);
   CHECK(plan[0].src == MENU_BLUR_COPY, "the first pass reads the copy");
   for (i = 0; i < MENU_BLUR_PASSES; i++)
   {
      float w = plan[i].params[0] * 480.0f, h = plan[i].params[1] * 270.0f;
      if (i && plan[i].src != last)
         ok = false;
      if (     plan[i].add || plan[i].dst > 1 || plan[i].dst == plan[i].src
            || plan[i].params[2] != 0.0f || plan[i].params[3] != 1.0f
            || (w != 0.0f) == (h != 0.0f))
         ok = false;
      /* Each round twice as wide as the last, the first at the copy's
       * texel */
      if (TEST_NEAR(w + h, 0.0f) || !TEST_NEAR(w + h,
               i < 1 ? 0.5f : (float)(1u << (i / 2))))
         ok = false;
      last = plan[i].dst;
   }
   CHECK(ok && last == 0,
         "one axis a pass, each reading the last, widening, into image 0");
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

   ribbon_motion();
   bloom_plans();
   blur_plans();

   for (i = 0; i < 16; i++)
      white[i] = 1.0f;
   memset(&rec_ctx, 0, sizeof(rec_ctx));
   rec_ctx.draw                   = rec_draw;
   rec_ctx.draw_pipeline          = rec_pipeline;
   rec_ctx.effect_install         = rec_effect_install;
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
      mesh = make(NULL, 4, NULL, 0, GFX_MESH_TRIANGLE_STRIP, GFX_MESH_FLAG_POSITIONS);
      CHECK(mesh && mesh->positions, "the owner can fill a mesh in place");
      if (mesh)
      {
         CHECK(mesh->vertices[3].x == 0.0f && mesh->positions[7] == 0.0f,
               "unfilled mesh streams start at zero");
         gfx_display_mesh_free(mesh);
      }
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
   md.bloom = false;
   md.vertex_count = 0;
   md.time = test_time;
   gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
   CHECK(rec_draws == 1 && rec_vertices == 10, "two triangles are ten strip vertices, one draw");
   CHECK(rec_texture == 7, "TEXTURED draws with its texture");
   CHECK(   TEST_NEAR(rec_xy[0], 0.0f) && TEST_NEAR(rec_xy[1], 0.0f)
         && TEST_NEAR(rec_xy[2], 0.0f) && TEST_NEAR(rec_xy[3], 0.0f)
         && TEST_NEAR(rec_xy[4], 1.0f) && TEST_NEAR(rec_xy[5], 0.0f)
         && TEST_NEAR(rec_xy[6], 0.0f) && TEST_NEAR(rec_xy[7], 1.0f)
         && TEST_NEAR(rec_xy[8], 0.0f) && TEST_NEAR(rec_xy[9], 1.0f),
         "clip space -1..1 lands on 0..1, the first triangle a a b c c");
   CHECK(TEST_NEAR(rec_xy[18], 1.0f) && TEST_NEAR(rec_xy[19], 1.0f), "the last vertex is the far corner");
   CHECK(TEST_NEAR(rec_uv[18], 1.0f) && TEST_NEAR(rec_uv[19], 1.0f), "texture coordinates in 1/65535ths");
   CHECK(TEST_NEAR(rec_col[0], 1.0f) && TEST_NEAR(rec_col[1], 0.0f) && TEST_NEAR(rec_col[39], 128.0f / 255.0f),
         "colours from the vertices");

   /* tint, COLORED */
   reset();
   {
      float tint[16];
      for (i = 0; i < 16; i++) tint[i] = 0.5f;
      md.color = tint; md.program = GFX_MESH_PROGRAM_COLORED; md.texture = 7;
      gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
      CHECK(TEST_NEAR(rec_col[0], 0.5f) && TEST_NEAR(rec_col[3], 0.5f), "the tint multiplies the vertex colour");
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
      CHECK(rec_vertices == 5 && TEST_NEAR(rec_xy[8], 0.75f) && TEST_NEAR(rec_xy[9], 0.75f),
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
      CHECK(rec_vertices == 4 && TEST_NEAR(rec_xy[6], 1.0f) && TEST_NEAR(rec_xy[7], 1.0f), "a strip goes as it is");
      gfx_display_mesh_free(strip);
   }

   /* effects */
   {
      static gfx_display_effect_desc_t descs[4];
      const gfx_display_effect_t *fx[4];
      descs[0].flags = GFX_EFFECT_MESH | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON;
      descs[1].flags = GFX_EFFECT_MESH | GFX_EFFECT_BLEND_SCREEN;
      descs[2].flags = 0;
      descs[3].flags = GFX_EFFECT_YFLIP;
      for (i = 0; i < 4; i++)
         fx[i] = gfx_display_effect_install(&descs[i]);
      CHECK(fx[0] && fx[3] && fx[0] != fx[3]
            && gfx_display_effect_install(&descs[2]) == fx[2],
            "an effect installed twice is one effect");
      rec_installs = 0;
      md.program = GFX_MESH_PROGRAM_EFFECT; md.texture = 0;
      md.vertex_count = 0; md.bloom = false;
      for (i = 0; i < 4; i++)
      {
         reset();
         md.effect = fx[i]; md.color = white;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(rec_installs == 4,
               "every installed effect's program is made at the first effect draw, once");
         CHECK(rec_pipeline_calls == 1
               && rec_pipeline_effect == (void*)&descs[i]
               && rec_draws == rec_pipeline_calls,
               "each effect reaches its pipeline with its program, then draws");
         CHECK(rec_effect_vertex == gfx_display_mesh_fullscreen()->positions
               && rec_effect_vertices == 4,
               "the effect coordinates are the mesh it is drawn over");
      }
      {
         /* One mesh lent to two effects: the second draws its first part */
         gfx_display_mesh_t *shared = make(NULL, 8, NULL, 0,
               GFX_MESH_TRIANGLE_STRIP, GFX_MESH_FLAG_POSITIONS);
         CHECK(shared && !((uintptr_t)shared->positions
                  % GFX_DISPLAY_MESH_ALIGN),
               "mesh positions are aligned for a GPU to read in place");
         if (shared)
         {
            reset();
            md.effect = fx[1]; md.color = white;
            md.vertex_count = 4;
            gfx_display_mesh_draw(p_disp, NULL, vd, shared, &md);
            CHECK(rec_effect_vertex == shared->positions
                  && rec_effect_vertices == 4,
                  "an effect drawn over part of a mesh reads that part in place");
            md.vertex_count = 0;
            gfx_display_mesh_draw(p_disp, NULL, vd, shared, &md);
            CHECK(rec_effect_vertices == 8, "and 0 draws all of it");
            gfx_display_mesh_free(shared);
         }
      }
      CHECK(gfx_display_effect_coords(p_disp) == NULL,
            "and there are none outside the draw");
      reset();
      md.effect = fx[0];
      gfx_display_mesh_draw(p_disp, NULL, vd, mesh, &md);
      CHECK(rec_pipeline_calls == 0 && rec_draws == 0,
            "a mesh with no positions draws no effect");
      CHECK(TEST_NEAR(white[3], 1.0f), "the effect leaves the colour's alpha as it was");

      /* The driver goes, and a driver that makes no programs comes */
      gfx_display_effects_lost();
      rec_ctx.effect_install = NULL;
      reset();
      gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
      CHECK(rec_pipeline_calls == 0 && rec_draws == 0,
            "a driver that makes no programs draws no effect");
      gfx_display_effects_lost();
      rec_ctx.effect_install = rec_effect_install;
      rec_installs = 0;
      reset();
      gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
      CHECK(rec_installs == 4 && rec_pipeline_calls == 1,
            "the programs are made again after the driver goes");
   }

   {
      static const unsigned rates[] = { 30, 60, 144 };
      unsigned rate, frame;
      static gfx_display_effect_desc_t ribbon_desc;
      ribbon_desc.flags = GFX_EFFECT_MESH | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON;
      md.program = GFX_MESH_PROGRAM_EFFECT;
      md.effect  = gfx_display_effect_install(&ribbon_desc);
      md.vertex_count = 0;
      test_clock_reads = 0;
      for (rate = 0; rate < sizeof(rates) / sizeof(rates[0]); rate++)
      {
         p_disp->effect_start_time = 0;
         test_time = 1000000;
         md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         for (frame = 1; frame <= rates[rate]; frame++)
         {
            test_time = 1000000 + (retro_time_t)frame * 1000000 / rates[rate];
            md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         }
         CHECK(TEST_NEAR(rec_effect_time, 0.6f), "one second has the same phase at 30, 60 and 144 Hz");
         md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(TEST_NEAR(rec_effect_time, 0.6f), "a repeated timestamp does not advance the effect");
      }
      rec_ctx.bloom_begin = rec_bloom_begin;
      rec_ctx.bloom_end   = rec_bloom_end;
      rec_bloom_calls     = 0;
      md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
      CHECK(rec_bloom_calls == 0, "no glow unless the menu asks for it");
      md.bloom = true;
      md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
      CHECK(rec_bloom_calls == 1, "the glow is drawn when the menu asks for it");
      CHECK(TEST_NEAR(rec_effect_time, 0.6f) && TEST_NEAR(p_disp->effect_time, 0.6f),
            "bloom and base use the same phase despite time spent drawing");
      {
         /* A driver that composites: the next frame's glow is drawn by
          * the prepass, outside the pass, and added in before the ribbon */
         unsigned draws;
         rec_ctx.bloom_composite = rec_bloom_composite;
         rec_bloom_check_time    = false;
         rec_bloom_calls = rec_bloom_deferred = rec_bloom_composites = 0;
         gfx_display_bloom_prepass(NULL, test_time);
         CHECK(rec_bloom_calls == 0, "no prepass before the ribbon asks for one");
         md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(rec_bloom_calls == 1 && rec_bloom_deferred == 0
               && rec_bloom_composites == 0,
               "the first frame draws its glow in place");
         gfx_display_bloom_prepass(NULL, test_time);
         CHECK(rec_bloom_calls == 2 && rec_bloom_deferred == 1,
               "the prepass draws the next frame's glow, deferred");
         draws = rec_draws;
         md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(rec_bloom_calls == 2 && rec_bloom_composites == 1
               && rec_bloom_draws_at_composite == draws,
               "the ribbon adds the ready glow in, before it is drawn");
         md.bloom = false;
         gfx_display_bloom_prepass(NULL, test_time);
         md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
         CHECK(rec_bloom_composites == 1, "nor is one added in without it");
         gfx_display_bloom_prepass(NULL, test_time);
         CHECK(rec_bloom_calls == 3, "a frame without the glow asks for none");
         rec_ctx.bloom_composite = NULL;
      }
      md.bloom = false;
      rec_ctx.bloom_begin = NULL;
      rec_ctx.bloom_end   = NULL;
      test_time = 500000;
      md.time = test_time;
         gfx_display_mesh_draw(p_disp, NULL, vd, gfx_display_mesh_fullscreen(), &md);
      CHECK(TEST_NEAR(rec_effect_time, 0.0f), "a clock reset restarts the effect");
      CHECK(test_clock_reads == 0,
            "the effects and the glow animate by the frame's time, not the clock");
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

   /* stretch: laid out at 320 wide, drawn across 2560 */
   {
      const unsigned layout = VIDEO_SCALE_PACK(320, 240);
      const unsigned output = VIDEO_SCALE_PACK(2560, 240);
      gfx_display_ctx_draw_t d;
      struct video_coords co;

      memset(&d, 0, sizeof(d));
      memset(&co, 0, sizeof(co));
      co.vertices                  = 4;
      co.color                     = white;
      d.coords                     = &co;
      d.pos                        = VIDEO_POS_PACK(-1, 20);
      d.dims                       = VIDEO_SCALE_PACK(3, 40);
      rec_ctx.handles_vertex_strip = false;
      p_disp->dispctx              = &rec_ctx;
      gfx_display_set_output_dims(p_disp, output);

      reset();
      gfx_display_draw_quad(p_disp, NULL, layout, 10, 20,
            VIDEO_SCALE_PACK(30, 40), layout, white, NULL);
      CHECK(rec_draws == 1 && rec_vd == output, "a quad reaches the driver at the output size");
      CHECK(rec_pos == VIDEO_POS_PACK(80, 180), "its x is stretched and its y is not");
      CHECK(rec_dims == VIDEO_SCALE_PACK(240, 40), "and so is its width");

      reset();
      gfx_display_draw(&rec_ctx, &d, NULL, layout);
      CHECK(rec_vd == output && rec_pos == VIDEO_POS_PACK(-8, 20)
            && rec_dims == VIDEO_SCALE_PACK(24, 40),
            "a draw is stretched, from left of the screen too");
      CHECK(d.pos == VIDEO_POS_PACK(-1, 20) && d.dims == VIDEO_SCALE_PACK(3, 40),
            "and the caller's draw is as it was");
      CHECK(gfx_display_output_dims(p_disp, layout) == output, "the driver is handed the output size");
      CHECK(gfx_display_output_dims(p_disp, output) == output, "drawing at the output size stretches nothing");

      rec_ctx.handles_vertex_strip = true;
      rec_ctx.media_shader_draw = rec_media_shader;
      reset();
      gfx_display_draw_quad(p_disp, NULL, layout, 10, 20,
            VIDEO_SCALE_PACK(30, 40), layout, white, NULL);
      CHECK(rec_draws == 0, "the menu background is queued");
      CHECK(gfx_display_draw_media_shader(&rec_ctx, NULL, 123,
               VIDEO_SCALE_PACK(160, 120), layout, d.pos, d.dims),
            "a supported viewer shader draws");
      CHECK(rec_vd == output && rec_pos == VIDEO_POS_PACK(-8, 20)
            && rec_dims == VIDEO_SCALE_PACK(24, 40),
            "viewer shaders use the ordinary thumbnail coordinate conversion");
      rec_ctx.media_shader_draw = NULL;
      CHECK(!gfx_display_draw_media_shader(&rec_ctx, NULL, 123,
               VIDEO_SCALE_PACK(160, 120), layout, d.pos, d.dims),
            "an unsupported backend keeps the plain thumbnail fallback");
      rec_ctx.handles_vertex_strip = false;

      gfx_display_set_output_dims(p_disp, 0);
      reset();
      gfx_display_draw(&rec_ctx, &d, NULL, layout);
      CHECK(rec_vd == layout && rec_dims == VIDEO_SCALE_PACK(3, 40), "cleared, nothing is stretched");
      CHECK(gfx_display_output_dims(p_disp, layout) == layout, "and the size is passed through");
      rec_ctx.handles_vertex_strip = true;
      p_disp->dispctx              = NULL;
   }

   /* blur */
   {
      const unsigned layout = VIDEO_SCALE_PACK(320, 240);
      const unsigned output = VIDEO_SCALE_PACK(640, 240);
      p_disp->dispctx  = &rec_ctx;
      rec_blur_result  = true;

      reset();
      rec_blurs = 0;
      CHECK(!gfx_display_draw_blur(p_disp, NULL, vd, 0, 0, vd, 1.0f, 0.5f)
            && rec_blurs == 0, "with no hook nothing is blurred");

      rec_ctx.blur_rect = rec_blur_rect;
      gfx_display_draw_quad(p_disp, NULL, vd, 10, 20,
            VIDEO_SCALE_PACK(30, 40), vd, white, NULL);
      CHECK(rec_draws == 0, "a quad is gathered, not drawn");
      CHECK(gfx_display_draw_blur(p_disp, NULL, vd, 4, 6,
               VIDEO_SCALE_PACK(100, 50), 2.0f, -1.0f),
            "a driver that blurs says so");
      CHECK(rec_blurs == 1 && rec_blur_draws == 1,
            "the quad under the rect is drawn before it is blurred");
      CHECK(rec_blur_vd == vd && rec_blur_x == 4 && rec_blur_y == 6
            && rec_blur_dims == VIDEO_SCALE_PACK(100, 50),
            "the rect reaches the driver as asked");
      CHECK(TEST_NEAR(rec_blur_alpha, 1.0f), "the alpha is held to 1");
      CHECK(TEST_NEAR(rec_blur_dim, 0.0f), "the dim is held to 0..1");

      CHECK(gfx_display_draw_blur(p_disp, NULL, vd, 0, 0, vd, 0.25f, 0.15f)
            && TEST_NEAR(rec_blur_alpha, 0.25f)
            && TEST_NEAR(rec_blur_dim, 0.15f), "a fading alpha and its dim are kept");
      CHECK(gfx_display_draw_blur(p_disp, NULL, vd, 0, 0, vd, 1.0f, 3.0f)
            && TEST_NEAR(rec_blur_dim, 1.0f), "a dim past 1 is held to 1");
      rec_blurs = 0;
      CHECK(!gfx_display_draw_blur(p_disp, NULL, vd, 0, 0, vd, 0.0f, 0.5f)
            && rec_blurs == 0, "no alpha, no blur");
      CHECK(!gfx_display_draw_blur(p_disp, NULL, vd, 0, 0,
               VIDEO_SCALE_PACK(7, 100), 1.0f, 0.5f)
            && !gfx_display_draw_blur(p_disp, NULL, vd, 0, 0,
               VIDEO_SCALE_PACK(100, 7), 1.0f, 0.5f)
            && rec_blurs == 0, "a rect under 8 pixels is not blurred");

      rec_blur_result = false;
      CHECK(!gfx_display_draw_blur(p_disp, NULL, vd, 0, 0, vd, 1.0f, 0.5f)
            && rec_blurs == 1, "a driver that cannot blur says so");
      rec_blur_result = true;

      gfx_display_set_output_dims(p_disp, output);
      CHECK(gfx_display_draw_blur(p_disp, NULL, layout, 10, 20,
               VIDEO_SCALE_PACK(30, 40), 1.0f, 0.5f),
            "a stretched rect is blurred");
      CHECK(rec_blur_vd == output && rec_blur_x == 20 && rec_blur_y == 20
            && rec_blur_dims == VIDEO_SCALE_PACK(60, 40),
            "across the output, its x and width stretched and its y not");
      gfx_display_set_output_dims(p_disp, 0);

      rec_ctx.blur_rect = NULL;
      p_disp->dispctx   = NULL;
      gfx_display_free();
   }

   if (fails)
   {
      printf("gfx_display_mesh_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("gfx_display_mesh_test: all checks passed\n");
   return 0;
}

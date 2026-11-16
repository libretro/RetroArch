#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#define SUCCEEDED(hr) ((hr) >= 0)
#define FAILED(hr) ((hr) < 0)
#define TRUE true
/* The drivers' mesh case is "case 1:"; the ribbon falls into it */
#define TEST_RIBBON_SIMPLE 1
#define TEST_RIBBON 2
#define D3DUSAGE_WRITEONLY 0
#define D3DPOOL_DEFAULT 0
#define D3DCOLOR_ARGB(a,r,g,b) 0xFFFFFFFFu
#define IDirect3DDevice9_SetRenderState(d,s,v) ((void)0)

typedef struct { float x,y,z,u,v; uint32_t color; } Vertex;
typedef struct buffer { Vertex *data; unsigned count; } *LPDIRECT3DVERTEXBUFFER9;
struct video_coords { const float *vertex; unsigned vertices; };
typedef struct { uint32_t id; unsigned vertex_count; } gfx_display_mesh_t;
typedef struct { const gfx_display_mesh_t *effect_mesh; } gfx_display_t;
typedef struct { struct video_coords *coords; unsigned which; } gfx_display_ctx_draw_t;
typedef struct { void *dev; } d3d9_video_t;

static unsigned creates, releases, locks, unlocks;
static bool fail_create, fail_lock, fail_unlock;
static LPDIRECT3DVERTEXBUFFER9 bound;
#include "ribbon_state.inc"

static int create_buffer(void *dev, unsigned bytes, unsigned usage,
      unsigned fvf, unsigned pool, LPDIRECT3DVERTEXBUFFER9 *out, void *shared)
{
   (void)dev; (void)usage; (void)fvf; (void)pool; (void)shared;
   creates++;
   *out = NULL;
   if (fail_create)
      return -1;
   *out = (LPDIRECT3DVERTEXBUFFER9)malloc(sizeof(**out));
   assert(*out);
   (*out)->data = (Vertex*)calloc(1, bytes);
   assert((*out)->data);
   (*out)->count = bytes / sizeof(Vertex);
   return 0;
}

static int lock_buffer(LPDIRECT3DVERTEXBUFFER9 vb, unsigned offset,
      unsigned bytes, void **out, unsigned flags)
{
   (void)offset; (void)bytes; (void)flags;
   locks++;
   *out = vb->data;
   return fail_lock ? -1 : 0;
}

static int unlock_buffer(LPDIRECT3DVERTEXBUFFER9 vb)
{
   (void)vb;
   unlocks++;
   return fail_unlock ? -1 : 0;
}

static void release_buffer(LPDIRECT3DVERTEXBUFFER9 vb)
{
   releases++;
   if (bound == vb)
      bound = NULL;
   free(vb->data);
   free(vb);
}

static void bind_buffer(void *dev, unsigned stream,
      LPDIRECT3DVERTEXBUFFER9 vb, unsigned offset, unsigned stride)
{
   (void)dev; (void)stream; (void)offset;
   assert(stride == sizeof(Vertex));
   bound = vb;
}

#define IDirect3DDevice9_CreateVertexBuffer create_buffer
#define IDirect3DVertexBuffer9_Lock lock_buffer
#define IDirect3DVertexBuffer9_Unlock unlock_buffer
#define IDirect3DVertexBuffer9_Release release_buffer
#define IDirect3DDevice9_SetStreamSource bind_buffer

static void pipeline(gfx_display_ctx_draw_t *draw, gfx_display_t *p_disp,
      d3d9_video_t *d3d, struct video_coords *ca)
{
   (void)p_disp;
   draw->coords = ca;
   switch (draw->which)
   {
      case TEST_RIBBON:
#include "ribbon.inc"
   }
}

static void check_buffer(const struct video_coords *coords)
{
   unsigned i;
   assert(bound && bound->count == coords->vertices);
   for (i = 0; i < coords->vertices; i++)
   {
      assert(bound->data[i].x == coords->vertex[2 * i]);
      assert(bound->data[i].y == coords->vertex[2 * i + 1]);
   }
}

int main(void)
{
   unsigned i, mode;
   gfx_display_mesh_t mesh = {1, 0};
   gfx_display_t display = {&mesh};
   d3d9_video_t driver = {NULL};
   struct video_coords coords;
   gfx_display_ctx_draw_t draw;
   float *positions = (float*)malloc(8352 * 2 * sizeof(float));
   assert(positions);
   for (i = 0; i < 8352 * 2; i++)
      positions[i] = (float)i;
   coords.vertex = positions;
   coords.vertices = 8064;
   mesh.vertex_count = coords.vertices;
   draw.which = TEST_RIBBON_SIMPLE;
   pipeline(&draw, &display, &driver, &coords);
   check_buffer(&coords);
   pipeline(&draw, &display, &driver, &coords);
   assert(creates == 1 && locks == 1);

   mesh.id++;
   coords.vertices = 8352;
   mesh.vertex_count = coords.vertices;
   draw.which = TEST_RIBBON;
   pipeline(&draw, &display, &driver, &coords);
   check_buffer(&coords);
   assert(creates == 2 && locks == 2 && releases == 1);

   mesh.id++;
   positions[0] = -7.0f;
   pipeline(&draw, &display, &driver, &coords);
   check_buffer(&coords);
   assert(creates == 3 && locks == 3 && releases == 2);

   mesh.id++;
   coords.vertices = 8064;
   mesh.vertex_count = coords.vertices;
   draw.which = TEST_RIBBON_SIMPLE;
   pipeline(&draw, &display, &driver, &coords);
   check_buffer(&coords);

   for (mode = 0; mode < 3; mode++)
   {
      unsigned previous_unlocks = unlocks;
      mesh.id++;
      fail_create = mode == 0;
      fail_lock = mode == 1;
      fail_unlock = mode == 2;
      pipeline(&draw, &display, &driver, &coords);
      assert(draw.coords == NULL);
      assert(unlocks == previous_unlocks + (mode == 2));
      assert(coords.vertices == 8064);
      fail_create = fail_lock = fail_unlock = false;
      pipeline(&draw, &display, &driver, &coords);
      assert(draw.coords == &coords);
      check_buffer(&coords);
      assert(unlocks == previous_unlocks + (mode == 2) + 1);
   }
   release_buffer(pipeline_vbo);
   assert(releases + 1 == creates);
   free(positions);
   return 0;
}

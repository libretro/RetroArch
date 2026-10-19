/* Every shader pass's descriptor tables, after a preset is installed
 * either way over a cleared chain, are the slots made for that pass:
 * SLANG_NUM_BINDINGS side by side in each heap, owned by no other pass
 * and by no texture.
 * The production code runs against a fake heap (run.py extracts it). */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define GFX_MAX_SHADERS    64
#define SLANG_NUM_BINDINGS 16
#define SLANG_NUM_SEMANTICS 35
#define RARCH_WRAP_MAX     4
#define countof(a) (sizeof(a) / sizeof((a)[0]))

#define D3D12_FRAMES 2
#define SRV_SLOTS     (SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 2048 \
      + 2 * SLANG_NUM_BINDINGS * GFX_MAX_SHADERS \
      + (D3D12_FRAMES - 1) * 3 * SLANG_NUM_BINDINGS * GFX_MAX_SHADERS)
#define SAMPLER_SLOTS (SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 2 * RARCH_WRAP_MAX)
#define CPU_BASE 0x10000u
#define GPU_BASE 0x900000u
#define STRIDE   32u
#define TEXTURE  (2 * GFX_MAX_SHADERS + 1)
/* The owner of a slot of pass @i's second table */
#define ALT(i)   (GFX_MAX_SHADERS + 1 + (i))

typedef struct { size_t ptr; } D3D12_CPU_DESCRIPTOR_HANDLE;
typedef struct { size_t ptr; } D3D12_GPU_DESCRIPTOR_HANDLE;
typedef struct
{
   struct { unsigned NumDescriptors; } desc;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu;
   unsigned stride;
   int start;
   bool *map;
} d3d12_descriptor_heap_t;
typedef struct { D3D12_CPU_DESCRIPTOR_HANDLE rt_view; } texture_t;
typedef struct
{
   texture_t rt, feedback;
   D3D12_GPU_DESCRIPTOR_HANDLE textures, textures_alt, samplers;
} pass_t;
typedef struct
{
   struct
   {
      d3d12_descriptor_heap_t rtv_heap, srv_heap, sampler_heap;
      D3D12_CPU_DESCRIPTOR_HANDLE pass_tables_alt;
   } desc;
   struct { int renderTargets[2]; } chain;
   struct { pass_t pass[GFX_MAX_SHADERS]; } filter;
} driver_t;

static bool srv_map[SRV_SLOTS], sampler_map[SAMPLER_SLOTS];
/* The pass each slot was made for, TEXTURE for anything else */
static unsigned srv_owner[SRV_SLOTS], sampler_owner[SAMPLER_SLOTS];

#include "alloc.inc"

static void heap(d3d12_descriptor_heap_t *h, unsigned n, bool *map)
{
   memset(h, 0, sizeof(*h));
   h->desc.NumDescriptors = n;
   h->cpu.ptr             = CPU_BASE;
   h->gpu.ptr             = GPU_BASE;
   h->stride              = STRIDE;
   h->map                 = map;
}

static void init(driver_t *d3d12)
{
   unsigned i, j;
#include "init.inc"
}

static void install_deferred(driver_t *d3d12, unsigned i)
{
#include "deferred.inc"
}

static void install_set_shader(driver_t *d3d12, unsigned i)
{
#include "set_shader.inc"
}

/* Each pass's three tables lie on the slots made for it */
static void check(const driver_t *d3d12, const char *how)
{
   unsigned i, b;
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      size_t t = (d3d12->filter.pass[i].textures.ptr - GPU_BASE) / STRIDE;
      size_t a = (d3d12->filter.pass[i].textures_alt.ptr - GPU_BASE) / STRIDE;
      size_t s = (d3d12->filter.pass[i].samplers.ptr - GPU_BASE) / STRIDE;
      for (b = 0; b < SLANG_NUM_BINDINGS; b++)
      {
         if (     t + b >= SRV_SLOTS || srv_owner[t + b] != i + 1
               || a + b >= SRV_SLOTS || srv_owner[a + b] != ALT(i)
               || s + b >= SAMPLER_SLOTS || sampler_owner[s + b] != i + 1)
         {
            fprintf(stderr, "%s: pass %u binding %u lies on slot %u/%u/%u"
                  " (owners %u/%u/%u)\n", how, i, b,
                  (unsigned)(t + b), (unsigned)(a + b), (unsigned)(s + b),
                  t + b < SRV_SLOTS ? srv_owner[t + b] : 0,
                  a + b < SRV_SLOTS ? srv_owner[a + b] : 0,
                  s + b < SAMPLER_SLOTS ? sampler_owner[s + b] : 0);
            assert(0);
         }
      }
   }
}

int main(void)
{
   static driver_t d3d12;
   unsigned i, k;
   memset(&d3d12, 0, sizeof(d3d12));
   heap(&d3d12.desc.rtv_heap, 1024, srv_map);
   heap(&d3d12.desc.srv_heap, SRV_SLOTS, srv_map);
   heap(&d3d12.desc.sampler_heap, SAMPLER_SLOTS, sampler_map);
   init(&d3d12);
   /* What init made, by pass: the slots are taken in pass order */
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      size_t t = (d3d12.filter.pass[i].textures.ptr - GPU_BASE) / STRIDE;
      size_t a = (d3d12.filter.pass[i].textures_alt.ptr - GPU_BASE) / STRIDE;
      size_t s = (d3d12.filter.pass[i].samplers.ptr - GPU_BASE) / STRIDE;
      assert(d3d12.filter.pass[i].textures_alt.ptr);
      for (k = 0; k < SLANG_NUM_BINDINGS; k++)
      {
         assert(srv_map[t + k] && !srv_owner[t + k]);
         assert(sampler_map[s + k] && !sampler_owner[s + k]);
         srv_owner[t + k]     = i + 1;
         sampler_owner[s + k] = i + 1;
      }
      for (k = 0; k < SLANG_NUM_BINDINGS; k++)
      {
         assert(srv_map[a + k] && !srv_owner[a + k]);
         srv_owner[a + k]     = ALT(i);
      }
   }
   /* Then the textures the driver makes take the rest */
   for (k = 0; k < SRV_SLOTS; k++)
      if (!srv_owner[k])
         srv_owner[k] = TEXTURE;
   for (k = 0; k < SAMPLER_SLOTS; k++)
      if (!sampler_owner[k])
         sampler_owner[k] = TEXTURE;
   check(&d3d12, "init");
   /* A preset's install follows d3d12_chain_free(), which clears the
    * passes, the tables' handles with them */
   memset(d3d12.filter.pass, 0, sizeof(d3d12.filter.pass));
   for (i = 0; i < GFX_MAX_SHADERS; i++)
      install_deferred(&d3d12, i);
   check(&d3d12, "deferred install");
   memset(d3d12.filter.pass, 0, sizeof(d3d12.filter.pass));
   for (i = 0; i < GFX_MAX_SHADERS; i++)
      install_set_shader(&d3d12, i);
   check(&d3d12, "set_shader install");
   puts("D3D12 pass descriptor tables: own slots after init and both"
         " preset installs PASS");
   return 0;
}

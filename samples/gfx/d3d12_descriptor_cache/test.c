#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLANG_NUM_BINDINGS 16
#define D3D12_FRAMES 2
#define D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING 5768
#define D3D12_SRV_DIMENSION_TEXTURE2D 3
#define SWAP_RB_OPAQUE 6666
#define MENU_COMPONENTS 5313
#define RED_COMPONENTS 4096
typedef unsigned UINT;
typedef unsigned DXGI_FORMAT;
typedef struct { size_t ptr; } D3D12_CPU_DESCRIPTOR_HANDLE;
typedef D3D12_CPU_DESCRIPTOR_HANDLE D3D12_GPU_DESCRIPTOR_HANDLE;
typedef struct resource { unsigned refs, identity; } *D3D12Resource;
typedef struct
{
   D3D12Resource handle;
   struct { DXGI_FORMAT Format; UINT MipLevels; } desc;
   UINT srv_mapping;
} d3d12_texture_t;
typedef struct { d3d12_texture_t history[2]; } chain_t;
typedef struct
{
   DXGI_FORMAT Format;
   unsigned Shader4ComponentMapping, ViewDimension;
   struct { UINT MipLevels; } Texture2D;
} D3D12_SHADER_RESOURCE_VIEW_DESC;
typedef struct
{
   D3D12_CPU_DESCRIPTOR_HANDLE textures, textures_alt;
   struct d3d12_pass_desc_cache
   {
      D3D12Resource srv_resource[SLANG_NUM_BINDINGS];
      DXGI_FORMAT srv_format[SLANG_NUM_BINDINGS];
      UINT srv_mip_levels[SLANG_NUM_BINDINGS];
      UINT srv_mapping[SLANG_NUM_BINDINGS];
   } desc_cache[D3D12_FRAMES];
} pass_t;
static unsigned writes, descriptor_identity, descriptor_mapping;
static size_t last_handle;
typedef struct device device_t;
struct device
{
   struct vtable
   {
      void (*CreateShaderResourceView)(device_t*, D3D12Resource,
            const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
   } *lpVtbl;
};
typedef struct
{
   device_t *device;
   chain_t filter;
   struct { unsigned slot; } queue;
   struct
   {
      struct
      {
         D3D12_CPU_DESCRIPTOR_HANDLE cpu, gpu;
         unsigned stride;
      } srv_heap;
   } desc;
} driver_t;
#define AddRef(p) (++(p)->refs)
#define Release(p) do { if (p) { assert((p)->refs); --(p)->refs; (p) = NULL; } } while (0)
static void create_srv(device_t *device, D3D12Resource resource,
      const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
   (void)device;
   assert(desc->Texture2D.MipLevels);
   /* binding 2 of the first table, or of the second */
   assert(handle.ptr == 102 || handle.ptr == 152);
   last_handle = handle.ptr;
   descriptor_identity = resource ? resource->identity : 0;
   descriptor_mapping = desc->Shader4ComponentMapping;
   writes++;
}
static void bind_texture(driver_t *d3d12, pass_t *pass, chain_t *c,
      d3d12_texture_t *texture, D3D12Resource frame_res)
{
   struct { void *texture_data; unsigned binding; } semantic, *texture_sem;
   semantic.texture_data = texture;
   semantic.binding = 2;
   texture_sem = &semantic;
   {
#include "select.inc"
      {
#include "cache.inc"
      }
   }
}
static void bind(driver_t *d3d12, pass_t *pass, D3D12Resource resource,
      DXGI_FORMAT format, UINT levels)
{
   chain_t chain;
   d3d12_texture_t texture;
   memset(&chain, 0, sizeof(chain));
   texture.handle = resource;
   texture.desc.Format = format;
   texture.desc.MipLevels = levels;
   texture.srv_mapping = 0;
   bind_texture(d3d12, pass, &chain, &texture, NULL);
}
static void clear(pass_t *pass)
{
   unsigned i = 0, j, k;
   /* The production teardown loop needs only the chain's pass array. */
   struct chain_view { pass_t *pass; } view;
   struct chain_view *c = &view;
   view.pass = pass;
#include "release.inc"
}
int main(void)
{
   struct resource pool[2] = {{1, 1}, {0, 0}};
   struct vtable vtable = {create_srv};
   device_t device = {&vtable};
   driver_t driver;
   pass_t pass;
   D3D12Resource owner = &pool[0], replacement;
   memset(&driver, 0, sizeof(driver));
   memset(&pass, 0, sizeof(pass));
   driver.device = &device;
   driver.desc.srv_heap.cpu.ptr = 100;
   driver.desc.srv_heap.gpu.ptr = 200;
   driver.desc.srv_heap.stride = 1;
   pass.textures.ptr = 200;
   bind(&driver, &pass, owner, 28, 1);
   assert(writes == 1 && descriptor_identity == 1);
   bind(&driver, &pass, owner, 28, 1);
   assert(writes == 1 && descriptor_identity == 1);
   Release(owner);
   /* A live cache reference must prevent recycling this address. */
   replacement = pool[0].refs ? &pool[1] : &pool[0];
   replacement->refs = 1;
   replacement->identity = 2;
   bind(&driver, &pass, replacement, 28, 1);
   assert(writes == 2 && descriptor_identity == 2 && pool[0].refs == 0);
   bind(&driver, &pass, replacement, 28, 2);
   bind(&driver, &pass, replacement, 10, 2);
   assert(writes == 4 && replacement->refs == 2);
   clear(&pass);
   assert(replacement->refs == 1 && !pass.desc_cache[0].srv_resource[2]);
   bind(&driver, &pass, replacement, 10, 2);
   bind(&driver, &pass, NULL, 10, 2);
   assert(writes == 6 && replacement->refs == 1 && !descriptor_identity);
   clear(&pass);
   Release(replacement);
   assert(pool[0].refs == 0 && pool[1].refs == 0);
   {
      struct resource resources[2] = {{1, 3}, {1, 4}};
      chain_t chain;
      d3d12_texture_t tmp;
      unsigned base = writes;
      memset(&chain, 0, sizeof(chain));
      chain.history[0].handle = &resources[0];
      chain.history[0].desc.Format = 28;
      chain.history[0].desc.MipLevels = 1;
      bind_texture(&driver, &pass, &chain, chain.history, NULL);
      assert(writes == base + 1);
      assert(descriptor_mapping == D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
      chain.history[0].srv_mapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      bind_texture(&driver, &pass, &chain, chain.history, NULL);
      assert(writes == base + 1);
      chain.history[0].srv_mapping = SWAP_RB_OPAQUE;
      bind_texture(&driver, &pass, &chain, chain.history, NULL);
      assert(writes == base + 2 && descriptor_mapping == SWAP_RB_OPAQUE);
      bind_texture(&driver, &pass, &chain, chain.history, NULL);
      assert(writes == base + 2 && resources[0].refs == 2);
      chain.history[1] = chain.history[0];
      chain.history[1].handle = &resources[1];
      chain.history[1].srv_mapping = MENU_COMPONENTS;
      tmp = chain.history[0];
      chain.history[0] = chain.history[1];
      chain.history[1] = tmp;
      bind_texture(&driver, &pass, &chain, chain.history, NULL);
      assert(writes == base + 3 && descriptor_mapping == MENU_COMPONENTS);
      assert(descriptor_identity == 4);
      bind_texture(&driver, &pass, &chain, chain.history + 1, NULL);
      assert(writes == base + 4 && descriptor_mapping == SWAP_RB_OPAQUE);
      driver.filter.history[0].desc.Format = 28;
      driver.filter.history[0].srv_mapping = RED_COMPONENTS;
      bind_texture(&driver, &pass, &chain, chain.history, &resources[0]);
      assert(writes == base + 5 && descriptor_mapping == RED_COMPONENTS);
      assert(descriptor_identity == 3);
      bind_texture(&driver, &pass, &chain, chain.history, &resources[0]);
      assert(writes == base + 5);
      driver.filter.history[0].srv_mapping = 0;
      bind_texture(&driver, &pass, &chain, chain.history, &resources[0]);
      assert(writes == base + 6);
      assert(descriptor_mapping == D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
      clear(&pass);
      assert(resources[0].refs == 1 && resources[1].refs == 1);
   }
   {
      /* A frame of the second slot (D3D12_FRAMES) writes its own
       * table, which the GPU's reading of the first's does not see,
       * and keeps its own cache of it */
      struct resource r = {1, 7};
      unsigned base;
      memset(&pass, 0, sizeof(pass));
      pass.textures.ptr     = 200;
      pass.textures_alt.ptr = 250;
      base                  = writes;
      driver.queue.slot     = 0;
      bind(&driver, &pass, &r, 28, 1);
      assert(writes == base + 1 && last_handle == 102);
      driver.queue.slot     = 1;
      bind(&driver, &pass, &r, 28, 1);
      assert(writes == base + 2 && last_handle == 152);
      bind(&driver, &pass, &r, 28, 1);
      assert(writes == base + 2 && r.refs == 3);
      driver.queue.slot     = 0;
      bind(&driver, &pass, &r, 28, 1);
      assert(writes == base + 2);
      /* A pass without a second table stays on the first, its cache
       * with it */
      pass.textures_alt.ptr = 0;
      driver.queue.slot     = 1;
      bind(&driver, &pass, &r, 28, 1);
      assert(writes == base + 2);
      bind(&driver, &pass, &r, 10, 1);
      assert(writes == base + 3 && last_handle == 102);
      clear(&pass);
      assert(r.refs == 1);
      driver.queue.slot     = 0;
   }
   puts("D3D12 SRV cache: mapping, history, frame override, reuse,"
         " ownership and a cache a slot PASS");
   return 0;
}

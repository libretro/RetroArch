#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLANG_NUM_BINDINGS 16
#define D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING 5768
#define D3D12_SRV_DIMENSION_TEXTURE2D 3
typedef unsigned UINT;
typedef unsigned DXGI_FORMAT;
typedef struct { size_t ptr; } D3D12_CPU_DESCRIPTOR_HANDLE;
typedef struct resource { unsigned refs, identity; } *D3D12Resource;
typedef struct
{
   DXGI_FORMAT Format;
   unsigned Shader4ComponentMapping, ViewDimension;
   struct { UINT MipLevels; } Texture2D;
} D3D12_SHADER_RESOURCE_VIEW_DESC;
typedef struct
{
   D3D12_CPU_DESCRIPTOR_HANDLE textures;
   struct
   {
      D3D12Resource srv_resource[SLANG_NUM_BINDINGS];
      DXGI_FORMAT srv_format[SLANG_NUM_BINDINGS];
      UINT srv_mip_levels[SLANG_NUM_BINDINGS];
   } desc_cache;
} pass_t;
static unsigned writes, descriptor_identity;
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
   assert(handle.ptr == 102);
   descriptor_identity = resource ? resource->identity : 0;
   writes++;
}
static void bind(driver_t *d3d12, pass_t *pass, D3D12Resource resource,
      DXGI_FORMAT format, UINT levels)
{
   unsigned binding = 2;
#include "cache.inc"
}
static void clear(pass_t *pass)
{
   unsigned i = 0, j;
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
   assert(replacement->refs == 1 && !pass.desc_cache.srv_resource[2]);
   bind(&driver, &pass, replacement, 10, 2);
   bind(&driver, &pass, NULL, 10, 2);
   assert(writes == 6 && replacement->refs == 1 && !descriptor_identity);
   clear(&pass);
   Release(replacement);
   assert(pool[0].refs == 0 && pool[1].refs == 0);
   puts("D3D12 SRV cache: reuse, replacement, format, mips, null and teardown PASS");
   return 0;
}

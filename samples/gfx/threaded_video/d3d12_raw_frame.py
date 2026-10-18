#!/usr/bin/env python3
"""Exercise shipping D3D12 packed frame mapping and nonblocking loans."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")

def function(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

functions = next(line for line in source.splitlines()
                 if line.startswith("#define D3D12_FRAME_MAPPING_2101010 ")) + "\n"
for signature in ("static void d3d12_frame_mapping(",
                  "static unsigned d3d12_frame_bpp(",
                  "static void *d3d12_frame_mem_alloc(",
                  "static bool d3d12_get_current_software_framebuffer("):
    functions += function(signature)
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <libretro.h>
typedef unsigned DXGI_FORMAT;
typedef unsigned UINT;
typedef uint64_t UINT64;
#define DXGI_FORMAT_B5G6R5_UNORM 1
#define DXGI_FORMAT_B8G8R8X8_UNORM 2
#define DXGI_FORMAT_R10G10B10A2_UNORM 3
#define DXGI_FORMAT_R16G16B16A16_FLOAT 4
#define D3D12_ST_FLAG_SOURCE_HDR10 1
#define D3D12_SW_FB_SLOTS 3
#define D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(a,b,c,d) (4096u | (a) | ((b)<<3) | ((c)<<6) | ((d)<<9))
#define D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,1,2,3)
#define D3D12_SRV_DIMENSION_TEXTURE2D 3
struct fence;
struct fence_vtable { UINT64 (*GetCompletedValue)(struct fence*); };
struct fence { struct fence_vtable *lpVtbl; };
typedef struct fence *D3D12Fence;
typedef struct {
   unsigned Format, Shader4ComponentMapping, ViewDimension;
   struct { unsigned MostDetailedMip, MipLevels, PlaneSlice; float ResourceMinLODClamp; } Texture2D;
} D3D12_SHADER_RESOURCE_VIEW_DESC;
struct device;
struct device_vtable { void (*CreateShaderResourceView)(struct device*, void*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, unsigned); };
struct device { struct device_vtable *lpVtbl; };
typedef struct device *D3D12Device;
typedef struct {
   void *handle;
   struct { DXGI_FORMAT Format; unsigned MipLevels; } desc;
   UINT srv_mapping;
   unsigned cpu_descriptor[1];
} d3d12_texture_t;
typedef struct {
   D3D12Device device;
   DXGI_FORMAT format;
   unsigned flags;
   struct { D3D12Fence fence; } queue;
   struct {
      struct { size_t Offset; struct { unsigned RowPitch; } Footprint; } layout;
      unsigned last;
      UINT64 fence[D3D12_SW_FB_SLOTS];
      void *mapped[D3D12_SW_FB_SLOTS];
   } sw_fb;
} d3d12_video_t;
static unsigned writes, allocated, polls;
static UINT last_mapping;
static UINT64 completed;
static bool available = true;
static void create_srv(struct device *device, void *handle,
      const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, unsigned cpu)
{
   (void)device;
   assert(handle && cpu == 7);
   assert(desc->Format && desc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D);
   assert(desc->Texture2D.MipLevels == 1 && !desc->Texture2D.MostDetailedMip);
   assert(!desc->Texture2D.PlaneSlice && desc->Texture2D.ResourceMinLODClamp == 0);
   writes++;
   last_mapping = desc->Shader4ComponentMapping;
}
static UINT64 get_completed(struct fence *fence)
{
   (void)fence;
   polls++;
   return completed;
}
static bool d3d12_sw_fb_ensure(d3d12_video_t *video, unsigned w,
      unsigned h, DXGI_FORMAT format)
{
   assert(video && w && h && format);
   return available;
}
static void *d3d12_upload_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *align)
{
   assert(data && size);
   *id = 7;
   *cached = true;
   *align = 256;
   allocated++;
   return data;
}
FUNCTIONS
int main(void)
{
   struct device_vtable methods = { create_srv };
   struct device device = { &methods };
   struct fence_vtable fence_methods = { get_completed };
   struct fence fence = { &fence_methods };
   d3d12_texture_t texture, history[3];
   d3d12_video_t video;
   struct retro_framebuffer fb;
   unsigned char memory[3][512];
   uintptr_t id = 0;
   bool cached = false;
   unsigned align = 0, i;
   memset(&texture,0,sizeof(texture));
   texture.desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
   texture.desc.MipLevels = 1;
   texture.cpu_descriptor[0] = 7;
   d3d12_frame_mapping(&device,&texture,true);
   assert(texture.srv_mapping == D3D12_FRAME_MAPPING_2101010 && !writes);
   texture.handle = &texture;
   d3d12_frame_mapping(&device,&texture,false);
   assert(writes == 1 && !texture.srv_mapping && last_mapping == D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
   d3d12_frame_mapping(&device,&texture,false);
   assert(writes == 1);
   d3d12_frame_mapping(&device,&texture,true);
   assert(writes == 2 && last_mapping == D3D12_FRAME_MAPPING_2101010);
   for (i=0;i<3;i++) history[i] = texture;
   d3d12_frame_mapping(&device,&history[0],false);
   assert(!history[0].srv_mapping && history[1].srv_mapping == D3D12_FRAME_MAPPING_2101010);
   assert(history[2].srv_mapping == D3D12_FRAME_MAPPING_2101010);
   texture.desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
   d3d12_frame_mapping(&device,&texture,true);
   assert(!texture.srv_mapping && last_mapping == D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
   memset(&video,0,sizeof(video));
   memset(&fb,0,sizeof(fb));
   video.device = &device;
   video.format = DXGI_FORMAT_R10G10B10A2_UNORM;
   assert(d3d12_frame_mem_alloc(&video,256,&id,&cached,&align) == &video);
   assert(allocated == 1 && id == 7 && cached && align == 256);
   assert(!d3d12_frame_mem_alloc(&video,0,&id,&cached,&align));
   assert(!d3d12_frame_mem_alloc(NULL,256,&id,&cached,&align));
   video.device = NULL;
   assert(!d3d12_frame_mem_alloc(&video,256,&id,&cached,&align));
   fb.width = 64;
   fb.height = 1;
   video.sw_fb.layout.Footprint.RowPitch = 256;
   video.sw_fb.layout.Offset = 32;
   for (i=0;i<3;i++) video.sw_fb.mapped[i] = memory[i];
   assert(d3d12_get_current_software_framebuffer(&video,&fb));
   assert(fb.format == RETRO_PIXEL_FORMAT_XRGB2101010);
   assert(fb.pitch == 256 && fb.data == memory[1]+32 && fb.memory_flags == RETRO_MEMORY_TYPE_CACHED);
   video.flags = D3D12_ST_FLAG_SOURCE_HDR10;
   assert(d3d12_get_current_software_framebuffer(&video,&fb));
   assert(fb.format == RETRO_PIXEL_FORMAT_HDR10_2101010);
   video.queue.fence = &fence;
   video.sw_fb.fence[1] = video.sw_fb.fence[2] = 2;
   completed = 1;
   assert(!d3d12_get_current_software_framebuffer(&video,&fb) && polls == 1);
   video.sw_fb.fence[2] = 1;
   assert(d3d12_get_current_software_framebuffer(&video,&fb) && polls == 2);
   assert(fb.data == memory[2]+32);
   video.sw_fb.layout.Footprint.RowPitch = 512;
   assert(!d3d12_get_current_software_framebuffer(&video,&fb) && polls == 2);
   available = false;
   assert(!d3d12_get_current_software_framebuffer(&video,&fb));
   assert(!d3d12_get_current_software_framebuffer(NULL,&fb));
   assert(!d3d12_get_current_software_framebuffer(&video,NULL));
   puts("PASS: D3D12 raw 10-bit mappings, native reset, SDR/PQ loans and busy-fence fallback");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="retroarch-d3d12-raw-frame-") as directory:
    temp = Path(directory)
    c = temp / "test.c"
    c.write_text(fixture.replace("FUNCTIONS",functions),encoding="utf-8")
    cc = shlex.split(os.environ.get("CC","cc"))
    exe = temp / ("test.exe" if os.name == "nt" or "mingw" in cc[0] else "test")
    flags = ["-std=gnu89","-Wall","-Wextra","-Werror=declaration-after-statement",
             "-I"+str(root / "libretro-common/include")]
    subprocess.run(cc+flags+shlex.split(os.environ.get("CFLAGS",""))+[str(c),"-o",str(exe)],check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER",""))+[str(exe)],check=True)

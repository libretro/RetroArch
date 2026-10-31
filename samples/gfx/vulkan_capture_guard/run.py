#!/usr/bin/env python3
"""Exercise production readback guards and HDR cleanup without a GPU."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/vulkan.c').read_text()
sdr = source[source.index('static bool vulkan_read_viewport_internal('):]
hdr = source[source.index('static bool vulkan_read_viewport_hdr('):]
snapshot = '   caller_dims = vk->vp.dims;\n'
assert snapshot in sdr and snapshot in hdr
start = sdr.index('      vk->flags |= VK_FLAG_READBACK_PENDING;')
end = sdr.index('\n      {\n         /* Wait specifically', start)
sdr_guard = snapshot + sdr[start:end]
start = hdr.index(snapshot)
end = hdr.index('   if (!staging->memory)', start)
hdr_guard = hdr[start:end]
free = source[source.index('static void vulkan_free('):]
start = free.index('#ifdef VULKAN_HDR_SWAPCHAIN')
end = free.index('#endif /* VULKAN_HDR_SWAPCHAIN */', start)
cleanup = free[start:end] + '#endif\n'

program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#define VK_FLAG_READBACK_PENDING 1
#define VK_FLAG_READBACK_HDR 2
#define VK_CTX_FLAG_HDR_SUPPORT 4
#define VIDEO_FLAG_HDR_SUPPORT 1
#define VIDEO_FLAG_HDR10_SUPPORT 2
#define VIDEO_FLAG_SCRGB_SUPPORT 4
#define VULKAN_HDR_SWAPCHAIN 1
#define VK_NULL_HANDLE 0
#define VK_TRUE 1
typedef unsigned VkFence;
struct vk_texture { unsigned marker; };
struct context {
   unsigned device, flags, current_frame_index;
   VkFence swapchain_fences[1];
   bool swapchain_fences_signalled[1];
};
typedef struct {
   struct context *context;
   unsigned flags;
   struct { unsigned dims; } vp;
   struct { struct vk_texture staging[1]; } readback;
   struct { unsigned ubo, ubo_menu; } hdr;
   unsigned offscreen_buffer, readback_image;
} vk_t;
static vk_t *active;
static unsigned changed_dims, buffers, images;
static void video_driver_cached_frame(void) { active->vp.dims = changed_dims; }
static void vkWaitForFences(unsigned d, unsigned n, const VkFence *f, bool b, uint64_t t)
{ (void)d; (void)n; (void)f; (void)b; (void)t; }
static void vulkan_wait_own_submissions(vk_t *vk) { (void)vk; }
static void vulkan_destroy_buffer(unsigned d, unsigned *p)
{ (void)d; if (*p) buffers++; *p = 0; }
static void vulkan_destroy_hdr_buffer(unsigned d, unsigned *p)
{ (void)d; if (*p) images++; *p = 0; }
static void vulkan_deinit_hdr_readback_render_pass(vk_t *vk) { (void)vk; }
static void video_driver_modify_disp_flags(unsigned a, unsigned b) { (void)a; (void)b; }
static bool sdr_capture(vk_t *vk, unsigned char *buffer, bool is_idle)
{
   unsigned caller_dims;
''' + sdr_guard + r'''
   memset(buffer, 0, 24);
   return true;
}
static bool hdr_capture(vk_t *vk, unsigned char *buffer, bool is_idle)
{
   unsigned caller_dims, readback_slot;
   struct vk_texture *staging;
''' + hdr_guard + r'''
   (void)staging;
   memset(buffer, 0, 24);
   return true;
}
static void cleanup(vk_t *vk)
{
''' + cleanup + r'''
}
int main(void)
{
   struct context context;
   vk_t vk;
   unsigned mode, changed;
   unsigned char buffer[26];
   memset(&context, 0, sizeof(context));
   memset(&vk, 0, sizeof(vk));
   vk.context = &context;
   active = &vk;
   for (mode = 0; mode < 2; mode++)
      for (changed = 0; changed < 3; changed++)
      {
         bool result;
         unsigned i;
         vk.vp.dims = 0x20002;
         changed_dims = changed == 0 ? vk.vp.dims
            : (changed == 1 ? 0x40004 : 0x10001);
         memset(buffer, 0xa5, sizeof(buffer));
         result = mode ? hdr_capture(&vk, buffer + 1, false)
            : sdr_capture(&vk, buffer + 1, false);
         assert(result == (changed == 0));
         assert(buffer[0] == 0xa5 && buffer[25] == 0xa5);
         if (changed)
            for (i = 0; i < sizeof(buffer); i++) assert(buffer[i] == 0xa5);
         assert(!(vk.flags & VK_FLAG_READBACK_HDR));
      }
   for (mode = 0; mode < 2; mode++)
   {
      context.flags = mode ? VK_CTX_FLAG_HDR_SUPPORT : 0;
      vk.hdr.ubo = vk.hdr.ubo_menu = 1;
      vk.offscreen_buffer = vk.readback_image = mode;
      buffers = images = 0;
      cleanup(&vk);
      assert(buffers == 2 && images == mode * 2);
      cleanup(&vk);
      assert(buffers == 2 && images == mode * 2);
   }
   puts("PASS: resized SDR/BGRX and HDR captures reject before writes; HDR buffers freed on SDR and HDR contexts");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='vulkan-capture-guard-') as tmp:
    tmp = Path(tmp)
    src = tmp / 'test.c'
    src.write_text(program)
    exe = tmp / 'capture_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-Wall', '-Wextra', '-Werror', '-Wdeclaration-after-statement',
        '-I'+str(root / 'libretro-common/include'), str(src), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)

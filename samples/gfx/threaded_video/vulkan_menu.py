#!/usr/bin/env python3
"""Run shipping Vulkan menu publication against mapped-memory ownership checks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")
start = source.index("static void *vulkan_menu_frame(")
end = source.index("\n}\n", source.index("static void *vulkan_get_menu_framebuffer(", start)) + 3
functions = source[start:end]
for name in ["static bool vulkan_frame_mem_ready(", "static void vulkan_frame_mem_free(", "static void vulkan_menu_upload("]:
    start = source.index(name)
    functions += source[start:source.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <vulkan/vulkan.h>
#include "gfx/video_defines.h"
enum vk_texture_type { VULKAN_TEXTURE_STREAMED, VULKAN_TEXTURE_STAGING, VULKAN_TEXTURE_DYNAMIC };
#define VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT 1
#define VK_FRAME_MEM_MAX 20
#define VULKAN_MAX_SWAPCHAIN_IMAGES 3
#define VK_FLAG_MENU_ENABLE 1
struct vk_texture {
   void *mapped;
   VkDeviceMemory memory;
   VkImage image;
   VkBuffer buffer;
   VkFormat format;
   unsigned dims;
   size_t offset, stride, size;
   enum vk_texture_type type;
   unsigned flags;
};
struct context {
   VkDevice device; VkPhysicalDevice gpu; unsigned current_frame_index;
   bool swapchain_fences_signalled[3]; VkFence swapchain_fences[3];
};
typedef struct {
   struct context *context;
   VkCommandBuffer cmd;
   unsigned flags;
   struct vk_frame_mem { struct vk_texture tex; int index; bool menu; } frame_mem[VK_FRAME_MEM_MAX];
   struct {
      struct vk_texture textures[3], textures_optimal[3];
      struct vk_texture upload_tex;
      unsigned upload;
      void *loan;
      unsigned loan_dims, loan_index, last_index;
      float alpha;
      bool loan_rgb32, dirty[3];
   } menu;
} vk_t;
static uint8_t pixels[3][4096];
static bool mapped[3], fail_map, noncoherent;
static VkFormatFeatureFlags features16;
static enum vk_texture_type texture_type;
static unsigned copies, maps, unmaps, flushes, creates, gpu_copies, destroys;
static bool fence_ready;
VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence)
{
   (void)device; (void)fence;
   return fence_ready ? VK_SUCCESS : VK_NOT_READY;
}
static void vulkan_destroy_texture(VkDevice device, struct vk_texture *tex)
{
   (void)device;
   destroys++;
   memset(tex, 0, sizeof(*tex));
}
static void vulkan_texture_defer_copy(vk_t *vk, struct vk_texture *tex)
{
   (void)vk;
   destroys++;
   memset(tex, 0, sizeof(*tex));
}
static void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dst, struct vk_texture *src, unsigned x, unsigned y)
{
   (void)vk; (void)cmd;
   assert(dst->memory && src->buffer && !x && !y);
   assert(src->dims == VIDEO_SCALE_PACK(4, 3));
   assert(src->stride == (src->format == VK_FORMAT_B8G8R8A8_UNORM ? 16 : 8));
   gpu_copies++;
}
static unsigned memory_index(VkDeviceMemory memory)
{
   unsigned i = (unsigned)(uintptr_t)memory - 1;
   assert(i < 3);
   return i;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
      VkPhysicalDevice device, VkFormat format, VkFormatProperties *props)
{
   (void)device; (void)format;
   memset(props, 0, sizeof(*props));
   props->optimalTilingFeatures = features16;
}
VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice device,
      VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
      VkMemoryMapFlags flags, void **out)
{
   unsigned i = memory_index(memory);
   (void)device; (void)offset; (void)flags;
   assert(!mapped[i] && !offset && size == VK_WHOLE_SIZE);
   if (fail_map) return VK_ERROR_MEMORY_MAP_FAILED;
   mapped[i] = true;
   *out = pixels[i];
   maps++;
   return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
   unsigned i = memory_index(memory);
   (void)device;
   assert(mapped[i]);
   mapped[i] = false;
   unmaps++;
}
VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(VkDevice device,
      uint32_t count, const VkMappedMemoryRange *range)
{
   (void)device;
   assert(count == 1 && mapped[memory_index(range->memory)]);
   flushes++;
   return VK_SUCCESS;
}
static struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old, unsigned width, unsigned height,
      VkFormat fmt, const void *initial, const VkComponentMapping *swizzle,
      enum vk_texture_type type)
{
   struct vk_texture tex;
   unsigned index = vk->context->current_frame_index;
   (void)old; (void)initial; (void)swizzle;
   memset(&tex, 0, sizeof(tex));
   tex.memory = (VkDeviceMemory)(uintptr_t)(index + 1);
   tex.image = (VkImage)(uintptr_t)(index + 1);
   tex.format = fmt;
   tex.dims = VIDEO_SCALE_PACK(width, height);
   tex.stride = width * (fmt == VK_FORMAT_B4G4R4A4_UNORM_PACK16 ? 2 : 4) + 8;
   tex.size = tex.stride * height;
   tex.offset = 16;
   tex.type = type == VULKAN_TEXTURE_DYNAMIC ? type : texture_type;
   tex.flags = noncoherent ? VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT : 0;
   creates++;
   return tex;
}
static void *counted_copy(void *dst, const void *src, size_t n)
{
   copies++;
   return memcpy(dst, src, n);
}
#define memcpy counted_copy
FUNCTIONS
#undef memcpy
int main(void)
{
   vk_t vk;
   struct context context;
   unsigned mode, rgb, cache, y, x;
   size_t pitch;
   void *loan;
   memset(&context, 0, sizeof(context));
   for (mode = 0; mode < 2; mode++)
      for (rgb = 0; rgb < 2; rgb++)
         for (cache = 0; cache < 2; cache++)
         {
            unsigned bytes = rgb ? 4 : 2;
            unsigned before;
            memset(&vk, 0, sizeof(vk));
            memset(pixels, 0xcd, sizeof(pixels));
            vk.context = &context;
            features16 = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
               | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            noncoherent = cache != 0;
            texture_type = mode ? VULKAN_TEXTURE_STAGING : VULKAN_TEXTURE_STREAMED;
            copies = maps = unmaps = flushes = 0;
            loan = vulkan_get_menu_framebuffer(&vk, rgb != 0, VIDEO_SCALE_PACK(4, 3), &pitch);
            assert(loan == pixels[0] + 16 && pitch == 4 * bytes + 8);
            assert(!vk.menu.dirty[0] && maps == 1 && !unmaps);
            for (y = 0; y < 3; y++)
               memset((uint8_t*)loan + y * pitch, 0x37, 4 * bytes);
            before = creates;
            vulkan_set_texture_frame(&vk, loan, rgb != 0, VIDEO_SCALE_PACK(4, 3), 0.5f);
            assert(!copies && maps == 1 && unmaps == 1 && flushes == cache);
            assert(creates == before + mode);
            assert(!vk.menu.loan && !vk.menu.textures[0].mapped && !mapped[0]);
            assert(vk.menu.dirty[0] && vk.menu.alpha == 0.5f);
            for (y = 0; y < 3; y++)
               for (x = 0; x < pitch; x++)
                  assert(pixels[0][16 + y * pitch + x] == (x < 4 * bytes ? 0x37 : 0xcd));
         }
   for (mode = 0; mode < 4; mode++)
   {
      uint16_t input[12];
      features16 = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
      if (mode & 1) features16 |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
      if (mode & 2) features16 |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
      if (mode == 3) features16 = 0;
      creates = 0;
      assert(!vulkan_get_menu_framebuffer(&vk, false, VIDEO_SCALE_PACK(4, 3), &pitch));
      assert(!creates && !vk.menu.loan);
      for (x = 0; x < 12; x++) input[x] = 0xabcd;
      vulkan_set_texture_frame(&vk, input, false, VIDEO_SCALE_PACK(4, 3), 1.0f);
      pitch = vk.menu.textures[0].stride;
      for (y = 0; y < 3; y++)
         for (x = 0; x < 4; x++)
            assert(((uint32_t*)(pixels[0] + 16 + y * pitch))[x] == 0xd0c0b0a0);
   }
   features16 = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
               | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
   loan = vulkan_get_menu_framebuffer(&vk, true, VIDEO_SCALE_PACK(4, 3), &pitch);
   assert(loan);
   loan = vulkan_get_menu_framebuffer(&vk, false, VIDEO_SCALE_PACK(4, 3), &pitch);
   assert(loan && maps == unmaps + 1);
   /* A mismatched publication is abandoned without reading stale pixels. */
   vulkan_set_texture_frame(&vk, loan, true, VIDEO_SCALE_PACK(4, 3), 1.0f);
   assert(!vk.menu.loan && maps == unmaps);
   fail_map = true;
   assert(!vulkan_get_menu_framebuffer(&vk, true, VIDEO_SCALE_PACK(4, 3), &pitch));
   assert(!vk.menu.loan && maps == unmaps);
   fail_map = false;
   assert(!vulkan_get_menu_framebuffer(&vk, true, 0, &pitch));
   assert(!vulkan_get_menu_framebuffer(&vk, true, VIDEO_SCALE_PACK(4, 3), NULL));
   for (rgb = 0; rgb < 2; rgb++)
   {
      struct vk_frame_mem *fm;
      uint32_t input[12];
      memset(&vk, 0, sizeof(vk));
      vk.context = &context;
      context.swapchain_fences_signalled[0] = true;
      context.swapchain_fences[0] = (VkFence)(uintptr_t)1;
      fm = &vk.frame_mem[0];
      fm->menu = true;
      fm->index = -1;
      fm->tex.memory = (VkDeviceMemory)(uintptr_t)2;
      fm->tex.buffer = (VkBuffer)(uintptr_t)2;
      fm->tex.mapped = pixels[1];
      fm->tex.size = sizeof(pixels[1]);
      fm->tex.flags = VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT;
      fm->tex.type = VULKAN_TEXTURE_STAGING;
      mapped[1] = true;
      copies = maps = unmaps = flushes = gpu_copies = destroys = 0;
      vulkan_set_texture_frame(&vk, pixels[1], rgb != 0, VIDEO_SCALE_PACK(4, 3), 0.75f);
      assert(!copies && !maps && !unmaps && flushes == 1);
      assert(vk.menu.upload == 1 && fm->index == -2);
      fence_ready = true;
      assert(!vulkan_frame_mem_ready(&vk, 1));
      vulkan_menu_upload(&vk, 0);
      assert(!gpu_copies && !vulkan_frame_mem_ready(&vk, 1));
      vk.flags = VK_FLAG_MENU_ENABLE;
      vulkan_menu_upload(&vk, 0);
      assert(gpu_copies == 1 && !vk.menu.upload && fm->index == 0);
      fence_ready = false;
      assert(!vulkan_frame_mem_ready(&vk, 1));
      fence_ready = true;
      assert(vulkan_frame_mem_ready(&vk, 1) && fm->index == -1);
      /* Replacement before submission releases an unconsumed upload. */
      vulkan_set_texture_frame(&vk, pixels[1], rgb != 0, VIDEO_SCALE_PACK(4, 3), 1.0f);
      assert(!vulkan_frame_mem_ready(&vk, 1));
      memset(input, 0x31, sizeof(input));
      vulkan_set_texture_frame(&vk, input, true, VIDEO_SCALE_PACK(4, 3), 1.0f);
      assert(!vk.menu.upload && vulkan_frame_mem_ready(&vk, 1));
      vulkan_set_texture_frame(&vk, pixels[1], rgb != 0, VIDEO_SCALE_PACK(4, 3), 1.0f);
      fail_map = true;
      vulkan_set_texture_frame(&vk, input, true, VIDEO_SCALE_PACK(4, 3), 1.0f);
      fail_map = false;
      assert(!vk.menu.upload && !vk.menu.dirty[0] && vulkan_frame_mem_ready(&vk, 1));
      vulkan_menu_upload(&vk, 0);
      assert(gpu_copies == 1);
      vulkan_set_texture_frame(&vk, pixels[1], rgb != 0, VIDEO_SCALE_PACK(4, 3), 1.0f);
      vulkan_frame_mem_free(&vk, 1);
      assert(!vk.menu.upload && !vk.menu.dirty[0] && !fm->tex.mapped && destroys == 1);
      mapped[1] = false;
   }
   puts("PASS: Vulkan menu loans, padded rows, zero CPU copies, conversion refusal and mapped flushes");
   return 0;
}
'''
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-vulkan-menu-") as directory:
    directory = Path(directory)
    src = directory / "menu.c"
    exe = directory / ("menu.exe" if os.name == "nt" else "menu")
    src.write_text(fixture.replace("FUNCTIONS", functions), encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                   "-Werror=declaration-after-statement", "-I" + str(root),
                   "-I" + str(root / "gfx/include"), "-I" + str(root / "libretro-common/include"),
                   str(src), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)

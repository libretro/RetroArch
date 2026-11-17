#!/usr/bin/env python3
"""Compile the shipping device wrapper with controlled native creation results."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
parser.add_argument('--sanitizer', default='')
parser.add_argument('--runner', default='')
parser.add_argument('--artifacts', type=Path)
args = parser.parse_args()
here = Path(__file__).resolve().parent
repo = here.parents[2]
source = repo / 'gfx/common/vulkan_common.c'
text = source.read_text(encoding='utf-8')
start = text.index('static VkResult vulkan_context_create_device(')
end = text.index('\n#ifdef HAVE_OPENXR\n/* RetroArch\'s device extensions', start)
wrapper = text[start:end]
assert 'vulkan_context_create_device_plain' in wrapper
prefix = r'''
#define main device_plan_fixture_unused_main
#include DEVICE_PLAN_FIXTURE
#undef main
#define HAVE_THREADS 1
#define HAVE_RMP4 1
#define HAVE_RAC3 1
#define HAVE_OPENXR 1
#define VK_CTX_FLAG_TIMELINE_SEMAPHORE 1
typedef struct wrapper_context
{
   VkInstance instance, properties2_instance;
   VkPhysicalDeviceProperties gpu_properties;
   uint32_t instance_api_version, graphics_queue_index, recording_family, recording_queue_index;
   uint16_t flags;
   bool recording_device, recording_storage_images, recording_hevc;
   void *xr;
} wrapper_context_t;
typedef struct gfx_ctx_vulkan_data { wrapper_context_t context; } gfx_ctx_vulkan_data_t;
static gfx_ctx_vulkan_data_t ctx;
static unsigned created, augmented;
static bool refuse_augmentation, refuse_original, missing_proc;
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL wrapper_proc(VkInstance instance, const char *name)
{
   assert(instance);
   if (!strcmp(name, "vkGetPhysicalDeviceFeatures2")) return (PFN_vkVoidFunction)device_features;
   if (!strcmp(name, "vkGetPhysicalDeviceQueueFamilyProperties2")) return (PFN_vkVoidFunction)device_families;
   assert(!"unexpected native query");
   return NULL;
}
static PFN_vkGetInstanceProcAddr vulkan_symbol_wrapper_instance_proc_addr(void)
{
   return missing_proc ? NULL : wrapper_proc;
}
#define vkEnumerateDeviceExtensionProperties device_extensions
static VkResult vulkan_context_create_device_plain(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice physical, const VkDeviceCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkDevice *device)
{
   assert(vk == &ctx && physical && !allocator);
   created++;
   if (info != &request)
   {
      augmented++;
      assert(info->queueCreateInfoCount == 3 && info->pEnabledFeatures == &requested_features);
      assert(info->enabledExtensionCount == (hevc_extension && hevc_queue ? 7u : 6u));
      if (refuse_augmentation)
      {
         *device = (VkDevice)(uintptr_t)99;
         return VK_ERROR_FEATURE_NOT_PRESENT;
      }
   }
   else
   {
      assert(*device == VK_NULL_HANDLE);
      if (refuse_original)
         return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   }
   *device = (VkDevice)(uintptr_t)7;
   return VK_SUCCESS;
}
'''
suffix = r'''
static void wrapper_setup(void)
{
   setup();
   memset(&ctx, 0, sizeof(ctx));
   ctx.context.instance = ctx.context.properties2_instance = (VkInstance)(uintptr_t)1;
   ctx.context.instance_api_version = VK_API_VERSION_1_1;
   ctx.context.gpu_properties.apiVersion = VK_API_VERSION_1_3;
   ctx.context.flags = 128;
   created = augmented = 0;
   refuse_augmentation = refuse_original = missing_proc = false;
}
static VkResult wrapper_create(void)
{
   VkDevice device = VK_NULL_HANDLE;
   VkResult result = vulkan_context_create_device(&ctx, (VkPhysicalDevice)(uintptr_t)1, &request, NULL, &device);
   assert(!live && !memcmp(queues, saved_queues, sizeof(queues)));
   if (result == VK_SUCCESS) assert(device == (VkDevice)(uintptr_t)7);
   return result;
}
int main(void)
{
   wrapper_setup();
   hevc_extension = hevc_queue = true;
   assert(wrapper_create() == VK_SUCCESS && ctx.context.recording_hevc);
   wrapper_setup();
   hevc_extension = true;
   assert(wrapper_create() == VK_SUCCESS && !ctx.context.recording_hevc);
   wrapper_setup();
   assert(wrapper_create() == VK_SUCCESS && created == 1 && augmented == 1);
   assert(ctx.context.recording_device && ctx.context.recording_family == 2);
   assert(!ctx.context.recording_queue_index && !ctx.context.recording_storage_images);
   assert(ctx.context.flags == 129);
   wrapper_setup(); refuse_augmentation = true;
   assert(wrapper_create() == VK_SUCCESS && created == 2 && augmented == 1);
   assert(!ctx.context.recording_device && ctx.context.flags == 128);
   wrapper_setup(); refuse_augmentation = refuse_original = true;
   assert(wrapper_create() == VK_ERROR_OUT_OF_DEVICE_MEMORY && created == 2 && augmented == 1);
   assert(!ctx.context.recording_device);
   wrapper_setup(); ctx.context.xr = &ctx;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented && !allocations);
   assert(ctx.context.xr == &ctx && !ctx.context.recording_device);
   wrapper_setup(); ctx.context.properties2_instance = VK_NULL_HANDLE;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented && !allocations);
   wrapper_setup(); missing_proc = true;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented && !allocations);
   wrapper_setup(); ctx.context.instance_api_version = VK_API_VERSION_1_0;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented && !allocations);
   wrapper_setup(); fail_at = 1;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented);
   wrapper_setup();
   memset(&timeline, 0, sizeof(timeline));
   timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
   request.pNext = &timeline;
   assert(wrapper_create() == VK_SUCCESS && created == 1 && !augmented && !timeline.timelineSemaphore);
   puts("Vulkan renderer wrapper: optional failure fallback, caller ownership and instance guards PASS");
   return 0;
}
'''
temporary = tempfile.TemporaryDirectory(prefix='vulkan-device-wrapper-') if not args.artifacts else None
root = args.artifacts.resolve() if args.artifacts else Path(temporary.name)
root.mkdir(parents=True, exist_ok=True)
unit = root / 'wrapper.c'
unit.write_text(prefix.replace('DEVICE_PLAN_FIXTURE', json.dumps((here / 'device_plan_test.c').as_posix()))
                + wrapper + suffix)
compiler = shlex.split(args.cc)
flags = ['-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror', '-Wno-long-long',
         '-Wframe-larger-than=2048', '-O1', '-g1', '-I' + str(repo / 'libretro-common/include'),
         '-I' + str(repo / 'gfx/include')]
if 'clang' in subprocess.check_output(compiler + ['--version'], text=True).lower():
    flags += ['-Wno-c99-extensions']
if args.sanitizer:
    flags += ['-fsanitize=' + args.sanitizer, '-fno-omit-frame-pointer']
# .exe on every host, as a cross build to Windows needs it and the
# name costs nothing elsewhere
binary = root / 'wrapper.exe'
subprocess.run(compiler + flags + [str(unit), '-o', str(binary)], check=True, timeout=60)
subprocess.run(shlex.split(args.runner) + [str(binary)], check=True, timeout=15)
(root / 'evidence.json').write_text(json.dumps(dict(result='PASS',
    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    wrapper_sha256=hashlib.sha256(wrapper.encode()).hexdigest(),
    scope='Actual shipping wrapper body and device planner with controlled native callbacks; not a whole-renderer runtime'), indent=2) + '\n')
if temporary:
    temporary.cleanup()

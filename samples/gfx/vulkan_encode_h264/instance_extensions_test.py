#!/usr/bin/env python3
"""Compile the actual optional-extension selection for each platform profile."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cc', default='cc')
parser.add_argument('--runner', default='')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
source = (repo / 'gfx/common/vulkan_common.c').read_text(encoding='utf-8')
start = source.index('static const char *vulkan_optional_instance_extensions[]')
end = source.index('\n/* vkCreateInstance', start)
body = source[start:end]
prefix = r"""
#include <assert.h>
#include <string.h>
#undef _WIN32
#undef __WINRT__
#undef __APPLE__
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*(x)))
#define VULKAN_COLORSPACE_EXTENSION_NAME "VK_EXT_swapchain_colorspace"
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
typedef struct gfx_ctx_vulkan_data {
   struct { struct { int dxgi_present; } swapchain_settings; } context;
} gfx_ctx_vulkan_data_t;
"""
suffix = r"""
int main(void)
{
   gfx_ctx_vulkan_data_t vk;
   unsigned i, count;
   vk.context.swapchain_settings.dxgi_present = 1;
   assert(vulkan_optional_instance_count(&vk) == ARRAY_SIZE(vulkan_optional_instance_extensions));
   vk.context.swapchain_settings.dxgi_present = 0;
   count = vulkan_optional_instance_count(&vk);
   for (i = 0; i < ARRAY_SIZE(vulkan_optional_instance_extensions); i++)
   {
      const char *name = vulkan_optional_instance_extensions[i];
      int dxgi = !strcmp(name, "VK_KHR_external_memory_capabilities")
         || !strcmp(name, "VK_KHR_external_semaphore_capabilities");
      assert((i < count) == !dxgi);
   }
   return 0;
}
"""
profiles = [('windows-hdr', '#define _WIN32 1\n#define VULKAN_HDR_SWAPCHAIN 1\n'),
            ('windows-sdr', '#define _WIN32 1\n'),
            ('winrt-hdr', '#define _WIN32 1\n#define __WINRT__ 1\n#define VULKAN_HDR_SWAPCHAIN 1\n'),
            ('linux-hdr', '#define VULKAN_HDR_SWAPCHAIN 1\n'),
            ('apple-hdr', '#define __APPLE__ 1\n#define VULKAN_HDR_SWAPCHAIN 1\n')]
with tempfile.TemporaryDirectory(prefix='vulkan-instance-extensions-') as temp:
    root = Path(temp)
    for name, defines in profiles:
        unit = root / (name + '.c')
        unit.write_text(prefix + defines + body + suffix)
        binary = root / (name + '.exe')
        subprocess.run(shlex.split(args.cc) + ['-std=c89', '-pedantic', '-Wall',
                       '-Wextra', '-Werror', str(unit), '-o', str(binary)], check=True, timeout=30)
        subprocess.run(shlex.split(args.runner) + [str(binary)], check=True, timeout=10)
        print('PASS optional extensions ' + name)

    old = body.replace('count -= 2;', 'count -= 3;')
    assert old != body
    unit = root / 'old-count.c'
    guarded = suffix.replace('assert((i < count) == !dxgi);',
                             'if ((i < count) != !dxgi) return 1;')
    unit.write_text(prefix + profiles[0][1] + old + guarded)
    binary = root / 'old-count.exe'
    subprocess.run(shlex.split(args.cc) + ['-std=c89', '-pedantic', '-Wall',
                   '-Wextra', '-Werror', str(unit), '-o', str(binary)], check=True, timeout=30)
    result = subprocess.run(shlex.split(args.runner) + [str(binary)], check=False, timeout=10)
    assert result.returncode == 1, 'old extension truncation was not detected'
    print('PASS negative control: old DXGI truncation loses HDR colorspace')

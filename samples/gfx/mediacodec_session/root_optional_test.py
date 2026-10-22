#!/usr/bin/env python3
"""Check the shipping extension list/count preserves negotiated core devices."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/common/vulkan_common.c").read_text(encoding="utf-8")
start = source.index("static const char *vulkan_optional_device_extensions[]")
extensions = source[start:source.index("\n};", start) + 3]
start = source.index("static unsigned vulkan_optional_device_count(")
function = source[start:source.index("\n}", start) + 2]
header = (root / "gfx/common/vulkan_common.h").read_text(encoding="utf-8")
fallbacks = ""
for match in re.finditer(r'^#define (VK_[A-Z0-9_]+_EXTENSION_NAME) (.+)$', header, re.M):
    fallbacks += "#ifndef " + match[1] + "\n" + match[0] + "\n#endif\n"
unit = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gfx/common/vulkan_android_device.h"
#include "gfx/common/vulkan_prime_device.h"
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
typedef struct gfx_ctx_vulkan_data {
 struct {
  struct { bool dxgi_present; } swapchain_settings;
  bool xr;
 } context;
} gfx_ctx_vulkan_data_t;
#ifdef ANDROID
#undef _WIN32
#endif
''' + fallbacks + extensions + "\n" + function + r'''
int main(void)
{
 gfx_ctx_vulkan_data_t context;
 unsigned own,core,j;
 memset(&context,0,sizeof(context));
 own=vulkan_optional_device_count(&context,true);
 core=vulkan_optional_device_count(&context,false);
 for(j=0;j<core;j++)assert(vulkan_optional_device_extensions[j] && *vulkan_optional_device_extensions[j]);
#ifdef ANDROID
 {
  unsigned i,seen=0;
  assert(own>core);
  for(i=0;i<core;i++)assert(!vulkan_android_device_extension(vulkan_optional_device_extensions[i]));
  for(i=core;i<own;i++) {
   const char *n=vulkan_optional_device_extensions[i];
   assert(vulkan_android_device_extension(n));
   if(!strcmp(n,"VK_ANDROID_external_memory_android_hardware_buffer"))seen|=1;
   if(!strcmp(n,"VK_EXT_queue_family_foreign"))seen|=2;
   if(!strcmp(n,"VK_KHR_external_semaphore_fd"))seen|=4;
  }
  assert(seen==7);
 }
#elif defined(__linux__) && defined(HAVE_RMP4)
 {
  unsigned i,seen=0;
  assert(own==core+5);
  for(i=0;i<core;i++)assert(!vulkan_prime_device_extension(vulkan_optional_device_extensions[i]));
  for(i=core;i<own;i++)seen|=vulkan_prime_device_extension(vulkan_optional_device_extensions[i]);
  assert(seen==31);
  context.context.xr=true;
  assert(vulkan_optional_device_count(&context,true)==core);
 }
#else
 assert(own==core);
#endif
 puts("PASS: shipping optional imports preserve core devices, Linux XR and platform extension lists");
 return 0;
}
'''
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-Wno-long-long"]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True):
    flags += ["-Wno-c99-extensions"]
with tempfile.TemporaryDirectory(prefix="android-root-optional-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(unit, encoding="utf-8")
    for android in (False, True):
        for codec in ("HAVE_RMP4", "HAVE_RVP9", "HAVE_RAV1"):
            binary = path / (("android-" if android else "other-") + codec)
            command = compiler + flags + (["-DANDROID"] if android else [])
            command += ["-D" + codec, "-I" + str(root), "-I" + str(root / "gfx/include"),
                        "-I" + str(root / "libretro-common/include"), str(path / "test.c"),
                        str(root / "gfx/common/vulkan_android_device.c"), "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)

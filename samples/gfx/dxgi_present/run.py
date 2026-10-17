#!/usr/bin/env python3
"""Exercise the production DXGI pacing, submission and teardown with COM mocks."""
import os
import shlex
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/common/win_dxgi_present.c").read_text()
source = source[source.index("#define CINTERFACE"):]
start = source.rindex("unsigned ra_dxgi_bridge_index(")
end = source.index("void ra_dxgi_bridge_hdr_metadata(", start)
destroy_start = source.index("static int ra_dxgi_bridge_wait_idle(")
destroy_end = source.index("static int ra_dxgi_bridge_create_buffers(", destroy_start)
with tempfile.TemporaryDirectory(prefix="retroarch-dxgi-") as directory:
    directory = Path(directory)
    (directory / "present.inc").write_text(source[start:end])
    (directory / "destroy.inc").write_text(source[destroy_start:destroy_end])
    resize_start = source.index("int ra_dxgi_bridge_resize(")
    resize_end = source.index("ra_dxgi_bridge_t *ra_dxgi_bridge_create(", resize_start)
    (directory / "resize.inc").write_text(source[resize_start:resize_end])
    vulkan = (root / "gfx/common/win_vulkan_dxgi.c").read_text()
    acquire_start = vulkan.index("VkResult ra_vulkan_dxgi_acquire(")
    acquire_end = vulkan.index("VkResult ra_vulkan_dxgi_present(", acquire_start)
    (directory / "acquire.inc").write_text(vulkan[acquire_start:acquire_end])
    commands_start = vulkan.index("      command.sType")
    commands_end = vulkan.index("      for (j = 0; j < 2; j++)", commands_start)
    allocate_start = vulkan.index("         if (*buffer == VK_NULL_HANDLE)", commands_end)
    allocate_end = vulkan.index("         begin.sType", allocate_start)
    (directory / "commands.inc").write_text(
        vulkan[commands_start:commands_end] + vulkan[allocate_start:allocate_end])
    wait_start = vulkan.rindex("static VkResult ra_vulkan_dxgi_drain(")
    wait_end = vulkan.index("int ra_vulkan_dxgi_resize(", wait_start)
    (directory / "wait.inc").write_text(vulkan[wait_start:wait_end])
    frontend = (root / "gfx/common/vulkan_common.c").read_text()
    drain_start = frontend.index("   vulkan_context_wait_frames(vk);", frontend.index("/* Replacing the swapchain or changing the acquire path"))
    drain_end = frontend.index("   vk->flags |= VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;", drain_start)
    (directory / "drain.inc").write_text(frontend[drain_start:drain_end])
    reuse_start = frontend.index("      if (vk->dxgi_presenter && ra_vulkan_dxgi_resize(")
    reuse_end = frontend.index("      old_swapchain = VK_NULL_HANDLE;", reuse_start)
    (directory / "reuse.inc").write_text(frontend[reuse_start:reuse_end])
    images_start = frontend.index("         for (i = 0; i < 2; i++)", reuse_end)
    images_end = frontend.index("         vk->context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;", images_start)
    (directory / "images.inc").write_text(frontend[images_start:images_end])
    wgl = (root / "gfx/common/win_wgl_dxgi.c").read_text()
    wgl_start = wgl.index("static void ra_wgl_dxgi_destroy_images(")
    wgl_end = wgl.index("int ra_wgl_dxgi_resize(", wgl_start)
    (directory / "wgl_images.inc").write_text(wgl[wgl_start:wgl_end])
    for name in ("test", "teardown", "acquire", "wait", "frontend", "wgl"):
        executable = directory / (name + ".exe")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c89", "-pedantic", "-Wall",
                        "-Wextra", "-Werror", "-I" + str(directory),
                        str(Path(__file__).with_name(name + ".c")), "-o", str(executable)], check=True)
        subprocess.run(shlex.split(os.environ.get("TEST_RUNNER", "")) + [str(executable)], check=True)

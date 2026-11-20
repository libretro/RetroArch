/* Copyright (C) 2026 The RetroArch team. GPL-3.0-or-later. */
#include <assert.h>
#include <string.h>
#include "../../../gfx/video_driver.h"
#define HAVE_RECORD_NVENC
#define HAVE_RECORD_VULKAN
#define string_is_equal(a,b) (strcmp((a),(b)) == 0)
static video_device_info_t offered_device;
static bool available;
bool video_driver_get_device(video_device_info_t *device)
{
   *device = offered_device;
   return available;
}
#include "resolve_fragment.c"
int main(void)
{
   assert(!strcmp(recording_capture_driver("nvenc"), "nvenc"));
   available = true;
   offered_device.api = VIDEO_DEVICE_D3D12;
   assert(!strcmp(recording_capture_driver("nvenc"), "nvenc"));
   offered_device.api = VIDEO_DEVICE_VULKAN;
   assert(!strcmp(recording_capture_driver("nvenc"), "nvenc"));
   offered_device.recording_caps = VIDEO_DEVICE_CAP_VULKAN_ENCODE;
   assert(!strcmp(recording_capture_driver("nvenc"), "nvenc"));
   offered_device.recording = &offered_device;
   assert(!strcmp(recording_capture_driver("nvenc"), "vulkan"));
   assert(!strcmp(recording_capture_driver("ffmpeg"), "ffmpeg"));
   assert(!strcmp(recording_capture_driver("pyrowave"), "pyrowave"));
   return 0;
}

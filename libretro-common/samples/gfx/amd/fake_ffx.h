/* What the stand-in FidelityFX API library (fake_ffx.c) saw: read by a
 * test through its exported fake_ffx_record(). Plain C, no SDK. */
#ifndef FAKE_FFX_H
#define FAKE_FFX_H

#include <stdint.h>

#define FAKE_FFX_MAX_CALLS 8

typedef struct fake_ffx_resource
{
   int present;
   void *resource;
   uint32_t type, format, width, height, mip_count, usage, state;
} fake_ffx_resource_t;

typedef struct fake_ffx_create
{
   uint32_t flags;
   uint32_t max_render_w, max_render_h, max_upscale_w, max_upscale_h;
   int has_message;
   uint64_t backend_type;       /* 2 D3D12, 3 Vulkan, 0 none */
   void *device;                /* the backend's device */
   void *physical_device;       /* Vulkan's */
   void *get_device_proc_addr;
   uint64_t version_id;         /* the override, 0 none */
   uint32_t api_version;        /* the version descriptor, 0 none */
} fake_ffx_create_t;

typedef struct fake_ffx_dispatch
{
   void *command_list;
   fake_ffx_resource_t color, depth, motion_vectors, exposure, reactive, transparency, output;
   float jitter_x, jitter_y, mv_scale_x, mv_scale_y;
   uint32_t render_w, render_h, upscale_w, upscale_h;
   int sharpening, reset;
   float sharpness, frame_time, pre_exposure, camera_near, camera_far, fov;
   uint32_t flags;
} fake_ffx_dispatch_t;

typedef struct fake_ffx_record
{
   unsigned creates, destroys, dispatches, queries, configures;
   fake_ffx_create_t create[FAKE_FFX_MAX_CALLS];
   fake_ffx_dispatch_t dispatch[FAKE_FFX_MAX_CALLS];
   uint32_t debug_level;
   /* the test's knobs */
   int create_fails;
   int dispatch_fails;
   int no_versions;             /* the version query finds none */
   int versions_need_device;    /* ...unless a device is given (Direct3D 12's library) */
   void *last_destroyed;
} fake_ffx_record_t;

#endif

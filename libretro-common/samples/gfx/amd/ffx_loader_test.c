/* gfx/amd/ffx_loader.c against a stand-in FidelityFX API library
 * (fake_ffx.c) in a directory of the test's: the descriptor layouts
 * pinned to the SDK's, the library found by each API's names, every
 * entry point resolved, the version query, a context made from a
 * chain the library reads as the SDK does, a dispatch's descriptor
 * read back whole. C89. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfx/amd/ffx_loader.h>
#include <file/file_path.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "fake_ffx.h"

/* The SDK's sizes and offsets on x86-64 (printed from its headers) */
#define LAYOUT(name, cond) typedef char layout_##name[(cond) ? 1 : -1]
LAYOUT(header,   sizeof(ffx_header_t) == 16);
LAYOUT(resource, sizeof(ffx_resource_t) == 48);
LAYOUT(create,   sizeof(ffx_upscale_create_t) == 48);
LAYOUT(create_m, offsetof(ffx_upscale_create_t, message) == 40);
LAYOUT(dispatch, sizeof(ffx_upscale_dispatch_t) == 432);
LAYOUT(disp_out, offsetof(ffx_upscale_dispatch_t, output) == 312);
LAYOUT(disp_jit, offsetof(ffx_upscale_dispatch_t, jitter_offset) == 360);
LAYOUT(disp_shp, offsetof(ffx_upscale_dispatch_t, enable_sharpening) == 392);
LAYOUT(disp_sh,  offsetof(ffx_upscale_dispatch_t, sharpness) == 396);
LAYOUT(disp_rst, offsetof(ffx_upscale_dispatch_t, reset) == 408);
LAYOUT(disp_nr,  offsetof(ffx_upscale_dispatch_t, camera_near) == 412);
LAYOUT(disp_fl,  offsetof(ffx_upscale_dispatch_t, flags) == 428);
LAYOUT(versions, sizeof(ffx_query_versions_t) == 56);
LAYOUT(override, sizeof(ffx_override_version_t) == 24);
LAYOUT(dx12,     sizeof(ffx_backend_dx12_t) == 24);
LAYOUT(vk,       sizeof(ffx_backend_vk_t) == 40);
LAYOUT(render,   sizeof(ffx_query_render_of_mode_t) == 48);
LAYOUT(debug,    sizeof(ffx_configure_debug_t) == 32);

static int failures;
#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

static fake_ffx_record_t *record_of(const ffx_lib_t *lib)
{
   fake_ffx_record_t *(*get)(void) = NULL;
#if defined(_WIN32)
   FARPROC p = GetProcAddress((HMODULE)lib->handle, "fake_ffx_record");
#else
   void *p = dlsym(lib->handle, "fake_ffx_record");
#endif
   memcpy(&get, &p, sizeof(get));
   return get ? get() : NULL;
}

/* The stand-in copied into @dir under @name (a run's own directory) */
static int plant(const char *dir, const char *name)
{
   char path[256];
   FILE *in, *out;
   char buf[4096];
   size_t n;
   path_mkdir(dir);
#if defined(_WIN32)
   in = fopen("fake_ffx.dll", "rb");
#else
   in = fopen("libfake_ffx.so", "rb");
#endif
   if (!in)
      return 0;
   sprintf(path, "%s/%s", dir, name);
   if (!(out = fopen(path, "wb")))
   {
      fclose(in);
      return 0;
   }
   while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
      fwrite(buf, 1, n, out);
   fclose(in);
   fclose(out);
   return 1;
}

static void messages(uint32_t type, const wchar_t *message)
{
   (void)type;
   (void)message;
}

int main(void)
{
   const ffx_lib_t *lib;
   fake_ffx_record_t *rec;
   uint64_t ids[4];
   const char *names[4];
   ffx_context_t ctx = NULL;
   ffx_upscale_create_t create;
   ffx_backend_dx12_t backend;
   ffx_override_version_t override;
   ffx_upscale_dispatch_t dispatch;
   ffx_query_render_of_mode_t render;
   ffx_configure_debug_t debug;
   uint32_t rw = 0, rh = 0;
   char path[256];
   int fake_device = 0, fake_list = 0, fake_color = 0, fake_out = 0;
#if defined(_WIN32)
   const char *d3d12_name = "amd_fidelityfx_dx12.dll";
   const char *vk_name = "amd_fidelityfx_vk.dll";
#else
   const char *d3d12_name = "libamd_fidelityfx_dx12.so";
   const char *vk_name = "libamd_fidelityfx_vk.so";
#endif

   /* (a previous run's) */
   sprintf(path, "ffx_loader_test_dir/%s", d3d12_name);
   remove(path);
   sprintf(path, "ffx_loader_test_dir/%s", vk_name);
   remove(path);

   /* Nothing in an empty or missing directory, and nothing loaded */
   CHECK(!ffx_runtime_in(FFX_API_D3D12, ""));
   CHECK(!ffx_runtime_in(FFX_API_D3D12, "ffx_loader_test_none"));
   CHECK(ffx_lib_get(FFX_API_D3D12, "ffx_loader_test_none") == NULL);
   CHECK(strstr(ffx_lib_status(FFX_API_D3D12), "no FSR library") != NULL);
   CHECK(strstr(ffx_lib_status(FFX_API_D3D12), "amd_fidelityfx_dx12") != NULL);
   CHECK(strstr(ffx_lib_status(FFX_API_D3D12), "ffx_loader_test_none") != NULL);
   CHECK(!strcmp(ffx_lib_status(FFX_API_VULKAN), "not probed"));

   /* The Vulkan library by its name, each entry point, the versions */
   CHECK(plant("ffx_loader_test_dir", vk_name));
   CHECK(ffx_runtime_in(FFX_API_VULKAN, "ffx_loader_test_dir"));
   CHECK(!ffx_runtime_in(FFX_API_D3D12, "ffx_loader_test_dir"));
   lib = ffx_lib_get(FFX_API_VULKAN, "ffx_loader_test_dir");
   CHECK(lib != NULL);
   if (!lib)
      return 1;
   CHECK(lib->create_context && lib->destroy_context && lib->configure && lib->query && lib->dispatch);
   CHECK(strstr(ffx_lib_status(FFX_API_VULKAN), vk_name) != NULL);
   /* (the same library after, whatever directory is asked) */
   CHECK(ffx_lib_get(FFX_API_VULKAN, "ffx_loader_test_none") == lib);
   rec = record_of(lib);
   CHECK(rec != NULL);
   if (!rec)
      return 1;
   CHECK(ffx_lib_versions(lib, NULL, ids, names, 4) == 2);
   CHECK(ids[0] == 0x4002 && ids[1] == 0x3104);
   CHECK(!strcmp(names[0], "FSR 4.0.2") && !strcmp(names[1], "FSR 3.1.4"));
   CHECK(ffx_lib_versions(lib, NULL, ids, names, 1) == 1 && ids[0] == 0x4002);
   rec->no_versions = 1;
   CHECK(ffx_lib_versions(lib, NULL, ids, names, 4) == 0);
   rec->no_versions = 0;
   rec->versions_need_device = 1;
   CHECK(ffx_lib_versions(lib, NULL, ids, names, 4) == 0);
   CHECK(ffx_lib_versions(lib, &fake_device, ids, names, 4) == 2);
   rec->versions_need_device = 0;

   /* The debug configuration, with no context */
   memset(&debug, 0, sizeof(debug));
   debug.header.type = FFX_TYPE_CONFIGURE_GLOBALDEBUG1;
   debug.message     = messages;
   debug.debug_level = 2;
   CHECK(lib->configure(NULL, &debug.header) == FFX_RETURN_OK && rec->debug_level == 2);

   /* The render size of a mode, with no context */
   memset(&render, 0, sizeof(render));
   render.header.type       = FFX_TYPE_UPSCALE_RENDER_OF_MODE;
   render.display_width     = 1920;
   render.display_height    = 1080;
   render.quality_mode      = FFX_UPSCALE_MODE_PERFORMANCE;
   render.out_render_width  = &rw;
   render.out_render_height = &rh;
   CHECK(lib->query(NULL, &render.header) == FFX_RETURN_OK && rw == 960 && rh == 540);

   /* A context from the chain: the upscaler's descriptor, the backend,
    * the version override */
   memset(&create, 0, sizeof(create));
   memset(&backend, 0, sizeof(backend));
   memset(&override, 0, sizeof(override));
   create.header.type         = FFX_TYPE_UPSCALE_CREATE;
   create.header.next         = &backend.header;
   create.flags               = FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
   create.max_render_size.width  = 1920;
   create.max_render_size.height = 1080;
   create.max_upscale_size.width  = 1920;
   create.max_upscale_size.height = 1080;
   create.message             = messages;
   backend.header.type        = FFX_TYPE_BACKEND_DX12;
   backend.header.next        = &override.header;
   backend.device             = &fake_device;
   override.header.type       = FFX_TYPE_OVERRIDE_VERSION;
   override.version_id        = 0x3104;
   CHECK(lib->create_context(&ctx, &create.header, NULL) == FFX_RETURN_OK && ctx != NULL);
   CHECK(rec->creates == 1);
   CHECK(rec->create[0].flags == (FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE));
   CHECK(rec->create[0].max_render_w == 1920 && rec->create[0].max_upscale_h == 1080);
   CHECK(rec->create[0].has_message);
   CHECK(rec->create[0].backend_type == FFX_TYPE_BACKEND_DX12 && rec->create[0].device == &fake_device);
   CHECK(rec->create[0].version_id == 0x3104);

   /* A dispatch: every field where the library expects it */
   memset(&dispatch, 0, sizeof(dispatch));
   dispatch.header.type              = FFX_TYPE_UPSCALE_DISPATCH;
   dispatch.command_list             = &fake_list;
   dispatch.color.resource           = &fake_color;
   dispatch.color.description.type   = FFX_RESOURCE_TEXTURE2D;
   dispatch.color.description.format = FFX_FORMAT_R8G8B8A8_UNORM;
   dispatch.color.description.width  = 1280;
   dispatch.color.description.height = 720;
   dispatch.color.description.mip_count = 1;
   dispatch.color.state              = FFX_STATE_COMPUTE_READ;
   dispatch.output.resource          = &fake_out;
   dispatch.output.description.usage = FFX_USAGE_UAV;
   dispatch.output.state             = FFX_STATE_UNORDERED_ACCESS;
   dispatch.jitter_offset.x          = 0.25f;
   dispatch.jitter_offset.y          = -0.125f;
   dispatch.motion_vector_scale.x    = 1.0f;
   dispatch.motion_vector_scale.y    = -1.0f;
   dispatch.render_size.width        = 1280;
   dispatch.render_size.height       = 720;
   dispatch.upscale_size.width       = 1920;
   dispatch.upscale_size.height      = 1080;
   dispatch.enable_sharpening        = 0;
   dispatch.sharpness                = 0.0f;
   dispatch.frame_time_delta         = 16.6f;
   dispatch.pre_exposure             = 1.0f;
   dispatch.reset                    = 1;
   dispatch.camera_near              = 0.1f;
   dispatch.camera_far               = 1000.0f;
   dispatch.camera_fov_angle_vertical = 1.0f;
   dispatch.flags                    = FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB;
   CHECK(lib->dispatch(&ctx, &dispatch.header) == FFX_RETURN_OK);
   CHECK(rec->dispatches == 1);
   CHECK(rec->dispatch[0].command_list == &fake_list);
   CHECK(rec->dispatch[0].color.present && rec->dispatch[0].color.resource == &fake_color
         && rec->dispatch[0].color.format == FFX_FORMAT_R8G8B8A8_UNORM
         && rec->dispatch[0].color.width == 1280 && rec->dispatch[0].color.height == 720
         && rec->dispatch[0].color.mip_count == 1 && rec->dispatch[0].color.state == FFX_STATE_COMPUTE_READ);
   CHECK(!rec->dispatch[0].depth.present && !rec->dispatch[0].exposure.present);
   CHECK(rec->dispatch[0].output.present && rec->dispatch[0].output.usage == FFX_USAGE_UAV
         && rec->dispatch[0].output.state == FFX_STATE_UNORDERED_ACCESS);
   CHECK(rec->dispatch[0].jitter_x == 0.25f && rec->dispatch[0].jitter_y == -0.125f);
   CHECK(rec->dispatch[0].mv_scale_x == 1.0f && rec->dispatch[0].mv_scale_y == -1.0f);
   CHECK(rec->dispatch[0].render_w == 1280 && rec->dispatch[0].upscale_h == 1080);
   CHECK(!rec->dispatch[0].sharpening && rec->dispatch[0].reset == 1);
   CHECK(rec->dispatch[0].frame_time == 16.6f && rec->dispatch[0].pre_exposure == 1.0f);
   CHECK(rec->dispatch[0].camera_near == 0.1f && rec->dispatch[0].camera_far == 1000.0f
         && rec->dispatch[0].fov == 1.0f);
   CHECK(rec->dispatch[0].flags == FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB);
   rec->dispatch_fails = 1;
   CHECK(lib->dispatch(&ctx, &dispatch.header) != FFX_RETURN_OK);
   rec->dispatch_fails = 0;
   {
      void *was = ctx;
      CHECK(lib->destroy_context(&ctx, NULL) == FFX_RETURN_OK && ctx == NULL);
      CHECK(rec->destroys == 1 && rec->last_destroyed == was);
   }
   rec->create_fails = 1;
   CHECK(lib->create_context(&ctx, &create.header, NULL) != FFX_RETURN_OK && ctx == NULL);
   rec->create_fails = 0;

   /* The Direct3D 12 library, by its SDK 1.1 name */
   CHECK(plant("ffx_loader_test_dir", d3d12_name));
   CHECK(ffx_runtime_in(FFX_API_D3D12, "ffx_loader_test_dir"));
   /* (the earlier miss did not stand: found now) */
   CHECK(ffx_lib_get(FFX_API_D3D12, "ffx_loader_test_dir") != NULL);
   CHECK(ffx_lib_get(FFX_API_D3D12, "ffx_loader_test_dir") != lib);
   CHECK(strstr(ffx_lib_status(FFX_API_D3D12), d3d12_name) != NULL);

   if (failures)
   {
      printf("ffx_loader_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("ffx_loader_test: ok\n");
   return 0;
}

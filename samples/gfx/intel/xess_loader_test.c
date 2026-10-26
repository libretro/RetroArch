/* gfx/intel/xess_loader.c against a stand-in XeSS library (fake_xess.c)
 * in a directory of the test's: the parameter layouts pinned to the
 * SDK's, the library found, every entry point of each API resolved,
 * the version, the optimal resolution of a quality, a context made,
 * initialised and executed on each API with its parameters read back
 * whole, the Vulkan extension and feature queries. C89. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfx/intel/xess_loader.h>
#include <file/file_path.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "fake_xess.h"

/* The SDK's sizes and offsets on x86-64 (printed from its headers) */
#define LAYOUT(name, cond) typedef char layout_##name[(cond) ? 1 : -1]
LAYOUT(version,    sizeof(xess_version_t) == 8);
LAYOUT(d3d12_init, sizeof(xess_d3d12_init_params_t) == 64);
LAYOUT(d3d12_exec, sizeof(xess_d3d12_execute_params_t) == 136);
LAYOUT(d3d12_jit,  offsetof(xess_d3d12_execute_params_t, jitter_offset_x) == 48);
LAYOUT(d3d12_base, offsetof(xess_d3d12_execute_params_t, input_color_base) == 72);
LAYOUT(d3d12_heap, offsetof(xess_d3d12_execute_params_t, descriptor_heap) == 120);
LAYOUT(vk_view,    sizeof(xess_vk_image_view_info_t) == 48);
LAYOUT(vk_format,  offsetof(xess_vk_image_view_info_t, format) == 36);
LAYOUT(vk_exec,    sizeof(xess_vk_execute_params_t) == 360);
LAYOUT(vk_jit,     offsetof(xess_vk_execute_params_t, jitter_offset_x) == 288);
LAYOUT(vk_init,    sizeof(xess_vk_init_params_t) == 64);
LAYOUT(vk_heap,    offsetof(xess_vk_init_params_t, temp_buffer_heap) == 24);

static int failures;
#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

static fake_xess_record_t *record_of(const xess_lib_t *lib)
{
   fake_xess_record_t *(*get)(void) = NULL;
#if defined(_WIN32)
   FARPROC p = GetProcAddress((HMODULE)lib->handle, "fake_xess_record");
#else
   void *p = dlsym(lib->handle, "fake_xess_record");
#endif
   memcpy(&get, &p, sizeof(get));
   return get ? get() : NULL;
}

/* The stand-in copied into @dir under the library's name */
static int plant(const char *dir)
{
   char path[256];
   FILE *in, *out;
   char buf[4096];
   size_t n;
   path_mkdir(dir);
#if defined(_WIN32)
   in = fopen("fake_xess.dll", "rb");
   sprintf(path, "%s/libxess.dll", dir);
#else
   in = fopen("libfake_xess.so", "rb");
   sprintf(path, "%s/libxess.so", dir);
#endif
   if (!in)
      return 0;
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

static int logged;
static void logger(const char *message, int level)
{
   (void)message;
   (void)level;
   logged++;
}

static void vk_view(xess_vk_image_view_info_t *v, uint64_t image, uint64_t view,
      uint32_t aspect, int format, uint32_t w, uint32_t h)
{
   memset(v, 0, sizeof(*v));
   v->image                         = image;
   v->image_view                    = view;
   v->subresource_range.aspect_mask = aspect;
   v->subresource_range.level_count = 1;
   v->subresource_range.layer_count = 1;
   v->format                        = format;
   v->width                         = w;
   v->height                        = h;
}

int main(void)
{
   const xess_lib_t *lib;
   fake_xess_record_t *rec;
   xess_context_t ctx = NULL;
   xess_version_t version;
   xess_2d_t out, optimal, min, max;
   xess_d3d12_init_params_t d3d12_init;
   xess_d3d12_execute_params_t d3d12_exec;
   xess_vk_init_params_t vk_init;
   xess_vk_execute_params_t vk_exec;
   int fake_device = 0, fake_list = 0, fake_instance = 0, fake_gpu = 0;
   int fake_color = 0, fake_mv = 0, fake_depth = 0, fake_out = 0;
   uint32_t count = 0, min_api = 0;
   const char *const *exts = NULL;
   void *features = NULL;

   /* Nothing in an empty or missing directory, and nothing loaded */
   CHECK(!xess_runtime_in(""));
   CHECK(!xess_runtime_in("xess_loader_test_none"));
   CHECK(!strcmp(xess_lib_status(), "not probed"));
   CHECK(xess_lib_get("") == NULL);
   CHECK(!strcmp(xess_lib_status(), "no XeSS directory"));
   CHECK(xess_lib_get("xess_loader_test_none") == NULL);
   CHECK(strstr(xess_lib_status(), "no XeSS library") != NULL);
   CHECK(strstr(xess_lib_status(), "xess_loader_test_none") != NULL);
   CHECK(xess_lib_get("xess_loader_test_none") == NULL);

   /* The library, every entry point, its version, in the status */
   CHECK(plant("xess_loader_test_dir"));
   CHECK(xess_runtime_in("xess_loader_test_dir"));
   lib = xess_lib_get("xess_loader_test_dir");
   CHECK(lib != NULL);
   if (!lib)
      return 1;
   CHECK(xess_lib_get("xess_loader_test_none") == lib);
   CHECK(lib->common.get_version && lib->common.get_optimal_input_resolution
         && lib->common.destroy_context && lib->common.set_jitter_scale
         && lib->common.set_velocity_scale && lib->common.set_exposure_multiplier
         && lib->common.set_logging_callback && lib->common.get_pipeline_build_status);
   CHECK(lib->d3d12.create_context && lib->d3d12.build_pipelines && lib->d3d12.init && lib->d3d12.execute);
   CHECK(lib->vk.get_required_instance_extensions && lib->vk.get_required_device_extensions
         && lib->vk.get_required_device_features && lib->vk.create_context
         && lib->vk.build_pipelines && lib->vk.init && lib->vk.execute);
   CHECK(lib->version.major == 2 && lib->version.minor == 0 && lib->version.patch == 1);
   CHECK(strstr(xess_lib_status(), "XeSS library ") == xess_lib_status());
   CHECK(strstr(xess_lib_status(), "xess_loader_test_dir") != NULL);
   CHECK(strstr(xess_lib_status(), " 2.0.1") != NULL);
   CHECK(strstr(xess_lib_status(), "Vulkan and Direct3D 12") != NULL);
   rec = record_of(lib);
   CHECK(rec != NULL);
   if (!rec)
      return 1;
   CHECK(lib->common.get_version(&version) == XESS_RESULT_SUCCESS && version.major == 2);

   /* Direct3D 12: a context, its pipelines, init, execute */
   CHECK(lib->d3d12.create_context(&fake_device, &ctx) == XESS_RESULT_SUCCESS && ctx != NULL);
   CHECK(rec->creates == 1 && rec->create_device == &fake_device);
   /* The optimal input resolution of a quality, of a context */
   out.x = 1920;
   out.y = 1080;
   CHECK(lib->common.get_optimal_input_resolution(ctx, &out, XESS_QUALITY_BALANCED,
            &optimal, &min, &max) == XESS_RESULT_SUCCESS);
   CHECK(optimal.x == 960 && optimal.y == 540);
   CHECK(min.x == 640 && min.y == 360 && max.x == 1920 && max.y == 1080);
   CHECK(lib->common.get_optimal_input_resolution(ctx, &out, XESS_QUALITY_AA,
            &optimal, &min, &max) == XESS_RESULT_SUCCESS && optimal.x == 1920);
   CHECK(XESS_FAILED(lib->common.get_optimal_input_resolution(ctx, &out, 99,
            &optimal, &min, &max)));

   CHECK(lib->common.set_logging_callback(ctx, XESS_LOGGING_LEVEL_WARNING, logger) == XESS_RESULT_SUCCESS);
   CHECK(rec->log_level == XESS_LOGGING_LEVEL_WARNING && logged == 1);
   CHECK(lib->d3d12.build_pipelines(ctx, NULL, false, XESS_INIT_FLAG_LDR_INPUT_COLOR) == XESS_RESULT_SUCCESS);
   CHECK(rec->pipelines == 1 && !rec->build_blocking);
   rec->build_pending = 2;
   CHECK(lib->common.get_pipeline_build_status(ctx) == XESS_RESULT_ERROR_OPERATION_IN_PROGRESS);
   CHECK(lib->common.get_pipeline_build_status(ctx) == XESS_RESULT_ERROR_OPERATION_IN_PROGRESS);
   CHECK(lib->common.get_pipeline_build_status(ctx) == XESS_RESULT_SUCCESS);
   memset(&d3d12_init, 0, sizeof(d3d12_init));
   d3d12_init.output_resolution.x = 1920;
   d3d12_init.output_resolution.y = 1080;
   d3d12_init.quality             = XESS_QUALITY_BALANCED;
   d3d12_init.init_flags          = XESS_INIT_FLAG_LDR_INPUT_COLOR | XESS_INIT_FLAG_INVERTED_DEPTH;
   d3d12_init.creation_node_mask  = 1;
   d3d12_init.visible_node_mask   = 1;
   /* (not initialised yet: an execute is refused) */
   memset(&d3d12_exec, 0, sizeof(d3d12_exec));
   d3d12_exec.color  = &fake_color;
   d3d12_exec.output = &fake_out;
   CHECK(lib->d3d12.execute(ctx, &fake_list, &d3d12_exec) == XESS_RESULT_ERROR_UNINITIALIZED);
   CHECK(lib->d3d12.init(ctx, &d3d12_init) == XESS_RESULT_SUCCESS);
   CHECK(rec->inits == 1 && !rec->init[0].vk && rec->init[0].context == ctx);
   CHECK(rec->init[0].out_w == 1920 && rec->init[0].out_h == 1080);
   CHECK(rec->init[0].quality == XESS_QUALITY_BALANCED);
   CHECK(rec->init[0].flags == (XESS_INIT_FLAG_LDR_INPUT_COLOR | XESS_INIT_FLAG_INVERTED_DEPTH));
   CHECK(rec->init[0].node_mask == 1);
   CHECK(lib->common.set_velocity_scale(ctx, 1.0f, -1.0f) == XESS_RESULT_SUCCESS);
   CHECK(rec->velocity_scale_x == 1.0f && rec->velocity_scale_y == -1.0f);
   CHECK(lib->common.set_jitter_scale(ctx, 2.0f, 2.0f) == XESS_RESULT_SUCCESS);
   CHECK(rec->jitter_scale_x == 2.0f);
   CHECK(lib->common.set_exposure_multiplier(ctx, 0.5f) == XESS_RESULT_SUCCESS);
   CHECK(rec->exposure_multiplier == 0.5f);
   d3d12_exec.velocity        = &fake_mv;
   d3d12_exec.depth           = &fake_depth;
   d3d12_exec.jitter_offset_x = 0.25f;
   d3d12_exec.jitter_offset_y = -0.125f;
   d3d12_exec.exposure_scale_value = 1.0f;
   d3d12_exec.reset_history   = 1;
   d3d12_exec.input_width     = 1129;
   d3d12_exec.input_height    = 635;
   CHECK(lib->d3d12.execute(ctx, &fake_list, &d3d12_exec) == XESS_RESULT_SUCCESS);
   CHECK(rec->executes == 1 && !rec->execute[0].vk && rec->execute[0].context == ctx);
   CHECK(rec->execute[0].command_list == &fake_list);
   CHECK(rec->execute[0].color.present && rec->execute[0].color.image == (uint64_t)(uintptr_t)&fake_color);
   CHECK(rec->execute[0].velocity.present && rec->execute[0].depth.present && rec->execute[0].output.present);
   CHECK(!rec->execute[0].exposure.present && !rec->execute[0].mask.present);
   CHECK(rec->execute[0].jitter_x == 0.25f && rec->execute[0].jitter_y == -0.125f);
   CHECK(rec->execute[0].exposure_scale == 1.0f && rec->execute[0].reset == 1);
   CHECK(rec->execute[0].input_w == 1129 && rec->execute[0].input_h == 635);
   rec->execute_fails = 1;
   CHECK(XESS_FAILED(lib->d3d12.execute(ctx, &fake_list, &d3d12_exec)));
   rec->execute_fails = 0;
   CHECK(lib->common.destroy_context(ctx) == XESS_RESULT_SUCCESS && rec->destroys == 1);
   ctx = NULL;
   rec->create_fails = 1;
   CHECK(lib->d3d12.create_context(&fake_device, &ctx) == XESS_RESULT_ERROR_UNSUPPORTED_DEVICE && ctx == NULL);
   rec->create_fails = 0;

   /* Vulkan: what the device needs, a context, init, execute */
   CHECK(lib->vk.get_required_instance_extensions(&count, &exts, &min_api) == XESS_RESULT_SUCCESS);
   CHECK(count == 1 && exts && !strcmp(exts[0], "VK_KHR_get_physical_device_properties2"));
   CHECK(min_api >= (1u << 22));
   CHECK(lib->vk.get_required_device_extensions(&fake_instance, &fake_gpu, &count, &exts) == XESS_RESULT_SUCCESS);
   CHECK(count == 2 && exts && !strcmp(exts[0], "VK_KHR_shader_float16_int8")
         && !strcmp(exts[1], "VK_KHR_16bit_storage"));
   CHECK(lib->vk.get_required_device_features(&fake_instance, &fake_gpu, &features) == XESS_RESULT_SUCCESS);
   CHECK(features != NULL && *(uint32_t*)features == 1000082000u);
   CHECK(lib->vk.create_context(&fake_instance, &fake_gpu, &fake_device, &ctx) == XESS_RESULT_SUCCESS && ctx != NULL);
   CHECK(rec->creates == 3 && rec->create_instance == &fake_instance
         && rec->create_physical_device == &fake_gpu && rec->create_device == &fake_device);
   CHECK(lib->vk.build_pipelines(ctx, 0, true, 0) == XESS_RESULT_SUCCESS);
   CHECK(rec->pipelines == 2 && rec->build_blocking);
   memset(&vk_init, 0, sizeof(vk_init));
   vk_init.output_resolution.x = 1280;
   vk_init.output_resolution.y = 720;
   vk_init.quality             = XESS_QUALITY_ULTRA_QUALITY;
   vk_init.init_flags          = XESS_INIT_FLAG_JITTERED_MV;
   rec->init_fails = 1;
   CHECK(XESS_FAILED(lib->vk.init(ctx, &vk_init)));
   rec->init_fails = 0;
   CHECK(lib->vk.init(ctx, &vk_init) == XESS_RESULT_SUCCESS);
   CHECK(rec->inits == 3 && rec->init[2].vk && rec->init[2].context == ctx);
   CHECK(rec->init[2].out_w == 1280 && rec->init[2].out_h == 720);
   CHECK(rec->init[2].quality == XESS_QUALITY_ULTRA_QUALITY && rec->init[2].flags == XESS_INIT_FLAG_JITTERED_MV);
   memset(&vk_exec, 0, sizeof(vk_exec));
   vk_view(&vk_exec.color,    0x1001, 0x2001, 1, 37, 853, 480);
   vk_view(&vk_exec.velocity, 0x1002, 0x2002, 1, 83, 853, 480);
   vk_view(&vk_exec.depth,    0x1003, 0x2003, 2, 126, 853, 480);
   vk_view(&vk_exec.output,   0x1004, 0x2004, 1, 37, 1280, 720);
   vk_exec.jitter_offset_x = -0.5f;
   vk_exec.jitter_offset_y = 0.5f;
   vk_exec.exposure_scale_value = 1.0f;
   vk_exec.input_width     = 853;
   vk_exec.input_height    = 480;
   CHECK(lib->vk.execute(ctx, &fake_list, &vk_exec) == XESS_RESULT_SUCCESS);
   CHECK(rec->executes == 3 && rec->execute[2].vk && rec->execute[2].context == ctx);
   CHECK(rec->execute[2].color.present && rec->execute[2].color.image == 0x1001
         && rec->execute[2].color.view == 0x2001 && rec->execute[2].color.aspect == 1
         && rec->execute[2].color.format == 37 && rec->execute[2].color.width == 853
         && rec->execute[2].color.height == 480);
   CHECK(rec->execute[2].depth.format == 126 && rec->execute[2].depth.aspect == 2);
   CHECK(rec->execute[2].output.image == 0x1004 && rec->execute[2].output.width == 1280);
   CHECK(!rec->execute[2].exposure.present && !rec->execute[2].mask.present);
   CHECK(rec->execute[2].jitter_x == -0.5f && rec->execute[2].jitter_y == 0.5f);
   CHECK(rec->execute[2].reset == 0 && rec->execute[2].input_w == 853);
   CHECK(lib->common.destroy_context(ctx) == XESS_RESULT_SUCCESS && rec->destroys == 2);
   CHECK(XESS_FAILED(lib->common.destroy_context(ctx)));

   if (failures)
   {
      printf("xess_loader_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("xess_loader_test: ok\n");
   return 0;
}

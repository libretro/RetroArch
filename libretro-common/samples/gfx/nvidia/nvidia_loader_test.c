/* gfx/nvidia: the NGX and NVAPI loaders against stand-in libraries.
 *
 * ngx_utf8_to_wide and ngx_dlss_feature_in need no library, nor does
 * ngx_lib_present on Windows. ngx_lib_get
 * finds the stand-in NGX core (fake_ngx.c, _nvngx.dll beside the test
 * on Windows, libnvidia-ngx.so.1 on LD_LIBRARY_PATH on Linux), resolves
 * every table and the test calls through them with the SDK's layouts:
 * init with a project id and a path list, the capability set, a
 * parameter set, a feature created, evaluated and released. nvapi_query
 * answers from fake_nvapi.c under Windows and NULL elsewhere. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <gfx/nvidia/ngx_loader.h>
#include <gfx/nvidia/nvapi_loader.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

static void test_wide(void)
{
   wchar_t w[8];
   CHECK(ngx_utf8_to_wide(w, 8, "abc") && w[0] == L'a' && w[2] == L'c' && w[3] == 0);
   CHECK(ngx_utf8_to_wide(w, 8, "") && w[0] == 0);
   /* U+00E9, two bytes */
   CHECK(ngx_utf8_to_wide(w, 8, "\xC3\xA9") && w[0] == 0xE9 && w[1] == 0);
   /* U+1F600, four bytes: a surrogate pair where wchar_t is 16-bit */
   CHECK(ngx_utf8_to_wide(w, 8, "\xF0\x9F\x98\x80"));
   if (sizeof(wchar_t) == 2)
      CHECK(w[0] == (wchar_t)0xD83D && w[1] == (wchar_t)0xDE00 && w[2] == 0);
   else
      CHECK(w[0] == (wchar_t)0x1F600 && w[1] == 0);
   /* does not fit: empty and false, never unterminated */
   CHECK(!ngx_utf8_to_wide(w, 4, "abcdef") && w[0] == 0);
   CHECK(!ngx_utf8_to_wide(w, 0, "a"));
   if (sizeof(wchar_t) == 2)
      CHECK(!ngx_utf8_to_wide(w, 2, "\xF0\x9F\x98\x80") && w[0] == 0);
}

static void test_feature_dir(void)
{
   char dir[DIR_MAX_LENGTH];
   char file[DIR_MAX_LENGTH];
   FILE *f;
   strcpy(dir, "nvidia_loader_test_dir");
   path_mkdir(dir);
   CHECK(!ngx_dlss_feature_in(NULL));
   CHECK(!ngx_dlss_feature_in(""));
   CHECK(!ngx_dlss_feature_in("nvidia_loader_test_missing"));
   fill_pathname_join_special(file, dir, "unrelated.txt", sizeof(file));
   f = fopen(file, "wb");
   if (f)
      fclose(f);
   CHECK(!ngx_dlss_feature_in(dir));
   remove(file);
#if defined(_WIN32)
   fill_pathname_join_special(file, dir, "nvngx_dlss.dll", sizeof(file));
#else
   fill_pathname_join_special(file, dir, "libnvidia-ngx-dlss.so.310.1.0", sizeof(file));
#endif
   f = fopen(file, "wb");
   if (f)
      fclose(f);
   CHECK(ngx_dlss_feature_in(dir));
   /* (an empty file: on Linux its name says the version, on Windows
    * nothing does) */
#if defined(_WIN32)
   CHECK(ngx_dlss_feature_version(dir) == 0);
#else
   CHECK(ngx_dlss_feature_version(dir) == NGX_VERSION_PACK(310, 1, 0));
#endif
   /* DLSS Neural Rendering's library beside it, never taken for
    * Super Resolution's nor it for that; its version never loads it */
   CHECK(!ngx_dlssnr_feature_in(dir));
   remove(file);
#if defined(_WIN32)
   fill_pathname_join_special(file, dir, "nvngx_dlssnr.dll", sizeof(file));
#else
   fill_pathname_join_special(file, dir, "libnvidia-ngx-dlssnr.so.310.8.0", sizeof(file));
#endif
   f = fopen(file, "wb");
   if (f)
      fclose(f);
   CHECK(ngx_dlssnr_feature_in(dir));
   CHECK(!ngx_dlss_feature_in(dir));
#if defined(_WIN32)
   CHECK(ngx_dlssnr_feature_version(dir) == 0);
#else
   CHECK(ngx_dlssnr_feature_version(dir) == NGX_VERSION_PACK(310, 8, 0));
#endif
   remove(file);
   CHECK(!ngx_dlssnr_feature_in(dir));
   CHECK(ngx_dlssnr_feature_version(dir) == 0);
   CHECK(!ngx_dlss_feature_in(dir));
   remove(dir);
   /* The stand-in feature library built beside the test: its version
    * read before NGX has it, from its file version (Windows) or file
    * name (Linux); without either, from the snippet word */
   CHECK(ngx_dlss_feature_in("nvidia_loader_test_feature"));
   CHECK(ngx_dlss_feature_version("nvidia_loader_test_feature") == NGX_VERSION_PACK(310, 2, 1));
   CHECK(ngx_dlss_feature_in("nvidia_loader_test_snippet"));
   CHECK(ngx_dlss_feature_version("nvidia_loader_test_snippet") == NGX_VERSION_PACK(310, 2, 2));
   /* The snippet word as the libraries pack it: 310.9.16 (its file
    * version says so), 3.5.0 */
   CHECK(ngx_snippet_version(0x01360910u) == NGX_VERSION_PACK(310, 9, 16));
   CHECK(ngx_snippet_version(0x00030500u) == NGX_VERSION_PACK(3, 5, 0));
   CHECK(NGX_VERSION_MAJOR(NGX_VERSION_PACK(310, 2, 1)) == 310
         && NGX_VERSION_MINOR(NGX_VERSION_PACK(310, 2, 1)) == 2
         && NGX_VERSION_PATCH(NGX_VERSION_PACK(310, 2, 1)) == 1);
   CHECK(ngx_dlss_feature_version("nvidia_loader_test_missing") == 0);
   CHECK(ngx_dlss_feature_version("") == 0);
}

/* OptiScaler in NGX's place: the module says so on Windows (the
 * stand-in core carries its product name there), and its parameter
 * tables carry its key on every platform */
static void test_optiscaler(const ngx_lib_t *lib, ngx_parameter_t *caps)
{
   void (*set)(int) = NULL;
#if defined(_WIN32)
   FARPROC p = GetProcAddress((HMODULE)lib->handle, "fake_ngx_set_optiscaler");
   CHECK(ngx_lib_is_optiscaler());
   CHECK(strstr(ngx_lib_status(), "(OptiScaler 0.7.7-test)") != NULL);
#else
   void *p = dlsym(lib->handle, "fake_ngx_set_optiscaler");
   CHECK(!ngx_lib_is_optiscaler());
   CHECK(strstr(ngx_lib_status(), "OptiScaler") == NULL);
#endif
   memcpy(&set, &p, sizeof(set));
   CHECK(set != NULL);
   CHECK(!ngx_params_from_optiscaler(lib, caps));
   CHECK(!ngx_params_from_optiscaler(lib, NULL));
   CHECK(!ngx_params_from_optiscaler(NULL, caps));
   if (set)
      set(1);
}

/* Found and named before anything loads it: a stand-in hooks the
 * process once loaded, so a status must not load it on Windows */
static void test_present(void)
{
#if defined(_WIN32)
   CHECK(ngx_lib_present() == (NGX_PRESENT_FOUND | NGX_PRESENT_OPTISCALER));
   CHECK(GetModuleHandleW(L"_nvngx.dll") == NULL);
#else
   CHECK(ngx_lib_present() == NGX_PRESENT_FOUND);
#endif
}

/* OptiScaler under its own name in the DLSS directory, loaded ahead of
 * the core library (Windows); nowhere else, and never after it */
static void test_stand_in(void)
{
#if defined(_WIN32)
   CHECK(ngx_stand_in_in("nvidia_loader_test_feature"));
   CHECK(!ngx_stand_in_in("nvidia_loader_test_snippet"));
   CHECK(!ngx_lib_load_stand_in("nvidia_loader_test_snippet"));
   CHECK(GetModuleHandleW(L"OptiScaler.dll") == NULL);
   CHECK(ngx_lib_load_stand_in("nvidia_loader_test_feature"));
   CHECK(GetModuleHandleW(L"OptiScaler.dll") != NULL);
   CHECK(ngx_lib_load_stand_in("nvidia_loader_test_feature"));
#else
   CHECK(!ngx_stand_in_in("nvidia_loader_test_feature"));
   CHECK(!ngx_lib_load_stand_in("nvidia_loader_test_feature"));
#endif
}

static void test_ngx(void)
{
   const ngx_lib_t *lib = ngx_lib_get();
   const wchar_t *paths[1];
   wchar_t dir[16];
   ngx_feature_common_info_t info;
   ngx_parameter_t *caps = NULL;
   ngx_parameter_t *p    = NULL;
   ngx_handle_t *h       = NULL;
   int device            = 1;
   int v                 = 0;
   int in_use            = 0;
   unsigned ic = 0, dc = 0;
   const char **ie = NULL, **de = NULL;

   CHECK(lib != NULL);
   if (!lib)
      return;
   CHECK(lib == ngx_lib_get());
   /* the status names the library and what it serves */
   CHECK(strstr(ngx_lib_status(), "NGX core library ") == ngx_lib_status()
         && (strstr(ngx_lib_status(), "nvngx") != NULL || strstr(ngx_lib_status(), "nvidia-ngx") != NULL)
         && strstr(ngx_lib_status(), "Vulkan") != NULL);
   CHECK(lib->param.set_i && lib->param.get_i && lib->param.set_void_pointer
         && lib->param.get_d3d12_resource);
   CHECK(lib->vulkan.init_project_id != NULL);
   CHECK(lib->vulkan.init_project_id_ext != NULL);
#if defined(_WIN32)
   CHECK(lib->d3d12.init_project_id != NULL);
#else
   CHECK(lib->d3d12.init_project_id == NULL);
   CHECK(lib->d3d12.evaluate_feature == NULL);
#endif

   CHECK(lib->vulkan.required_extensions(&ic, &ie, &dc, &de) == NGX_RESULT_SUCCESS);
   CHECK(ic == 1 && dc == 2 && de && !strcmp(de[1], "VK_NVX_image_view_handle"));

   /* not initialized yet */
   CHECK(NGX_FAILED(lib->vulkan.get_capability_parameters(&caps)));
   CHECK(lib->vulkan.get_capability_parameters(&caps) == NGX_RESULT_FAIL_NOT_INITIALIZED);

   ngx_utf8_to_wide(dir, 16, "Z");
   paths[0]              = dir;
   memset(&info, 0, sizeof(info));
   info.path_list.path   = paths;
   info.path_list.length = 1;
   CHECK(lib->vulkan.init_project_id("project-id", NGX_ENGINE_TYPE_CUSTOM,
         "1.0", dir, &device, &device, &device,
         NGX_VERSION_API, &info) == NGX_RESULT_SUCCESS);
   CHECK(NGX_FAILED(lib->vulkan.init_project_id(NULL, NGX_ENGINE_TYPE_CUSTOM,
         "1.0", dir, &device, &device, &device,
         NGX_VERSION_API, &info)));
   CHECK(lib->vulkan.init_project_id_ext("project-id", NGX_ENGINE_TYPE_CUSTOM,
         "1.0", dir, &device, &device, &device, NULL, NULL,
         NGX_VERSION_API, &info) == NGX_RESULT_SUCCESS);
   CHECK(NGX_SUCCEEDED(lib->vulkan.get_capability_parameters(&caps)) && caps);
   CHECK(lib->param.get_i(caps, NGX_PARAM_SUPER_SAMPLING_AVAILABLE, &v) == NGX_RESULT_SUCCESS && v == 1);
   /* the layouts crossed the boundary: the project id and the path list */
   CHECK(lib->param.get_i(caps, "Project", &v) == NGX_RESULT_SUCCESS && v == 10);
   CHECK(lib->param.get_i(caps, "Paths", &v) == NGX_RESULT_SUCCESS && v == 'Z');
   CHECK(NGX_FAILED(lib->param.get_i(caps, "Nothing", &v)));
   test_optiscaler(lib, caps);
   /* (the knob set: the next session's tables carry the key) */
   in_use = 1;
   CHECK(lib->vulkan.shutdown1(&device, &in_use) == NGX_RESULT_SUCCESS);
   CHECK(lib->vulkan.init_project_id_ext("project-id", NGX_ENGINE_TYPE_CUSTOM,
         "1.0", dir, &device, &device, &device, NULL, NULL,
         NGX_VERSION_API, &info) == NGX_RESULT_SUCCESS);
   CHECK(NGX_SUCCEEDED(lib->vulkan.get_capability_parameters(&caps)) && caps);
   CHECK(ngx_params_from_optiscaler(lib, caps));

   CHECK(lib->vulkan.allocate_parameters(&p) == NGX_RESULT_SUCCESS && p);
   CHECK(NGX_FAILED(lib->vulkan.create_feature1(&device, &device,
         NGX_FEATURE_SUPER_SAMPLING, p, &h)));
   lib->param.set_ui(p, NGX_PARAM_WIDTH, 640);
   lib->param.set_ui(p, NGX_PARAM_HEIGHT, 480);
   lib->param.set_f(p, NGX_PARAM_SHARPNESS, 0.5f);
   lib->param.set_void_pointer(p, NGX_PARAM_COLOR, &device);
   CHECK(lib->vulkan.create_feature1(&device, &device,
         NGX_FEATURE_SUPER_SAMPLING, p, &h) == NGX_RESULT_SUCCESS && h);
   CHECK(lib->vulkan.evaluate_feature(&device, h, p, NULL) == NGX_RESULT_SUCCESS);
   CHECK(lib->param.get_i(p, "Evaluated", &v) == NGX_RESULT_SUCCESS && v == 640);
   CHECK(lib->vulkan.release_feature(h) == NGX_RESULT_SUCCESS);
   CHECK(lib->vulkan.evaluate_feature(&device, h, p, NULL) == NGX_RESULT_FAIL_FEATURE_NOT_FOUND);
   CHECK(lib->vulkan.destroy_parameters(p) == NGX_RESULT_SUCCESS);
   in_use = 1;
   CHECK(lib->vulkan.shutdown1(&device, &in_use) == NGX_RESULT_SUCCESS && in_use == 0);
   CHECK(NGX_FAILED(lib->vulkan.get_capability_parameters(&caps)));

#if defined(_WIN32)
   CHECK(lib->d3d12.init_project_id("project-id", NGX_ENGINE_TYPE_CUSTOM,
         "1.0", dir, &device, NGX_VERSION_API, &info) == NGX_RESULT_SUCCESS);
   CHECK(lib->d3d12.allocate_parameters(&p) == NGX_RESULT_SUCCESS && p);
   lib->param.set_ui(p, NGX_PARAM_WIDTH, 320);
   lib->param.set_d3d12_resource(p, NGX_PARAM_COLOR, &device);
   CHECK(lib->d3d12.create_feature(&device, NGX_FEATURE_SUPER_SAMPLING, p, &h)
         == NGX_RESULT_SUCCESS && h);
   CHECK(lib->d3d12.evaluate_feature(&device, h, p, NULL) == NGX_RESULT_SUCCESS);
   CHECK(lib->param.get_i(p, "Evaluated", &v) == NGX_RESULT_SUCCESS && v == 320);
   CHECK(lib->d3d12.release_feature(h) == NGX_RESULT_SUCCESS);
   CHECK(lib->d3d12.destroy_parameters(p) == NGX_RESULT_SUCCESS);
   in_use = 1;
   CHECK(lib->d3d12.shutdown1(&device, &in_use) == NGX_RESULT_SUCCESS && in_use == 0);
#endif
}

static void test_nvapi(void)
{
#if defined(_WIN32)
   typedef int (__cdecl *sample_t)(int);
   sample_t sample = (sample_t)nvapi_query(0x12345678);
   CHECK(sample != NULL);
   if (sample)
      CHECK(sample(21) == 42);
   CHECK(nvapi_query(0x0150e828) != NULL);
#else
   CHECK(nvapi_query(0x12345678) == NULL);
#endif
   CHECK(nvapi_query(0xDEADBEEF) == NULL);
}

int main(void)
{
   test_wide();
   test_feature_dir();
   test_present();
   test_stand_in();
   test_ngx();
   test_nvapi();
   if (failures)
   {
      printf("nvidia_loader_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("nvidia_loader_test: ok\n");
   return 0;
}

#define HAVE_VULKAN
#define HAVE_EGL
#define HAVE_D3D10
#define HAVE_D3D11
#define HAVE_D3D12
#define HAVE_METAL

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "configuration.h"
#include "menu/menu_setting.h"

static settings_t settings;
static bool settings_available = true;
static unsigned warnings;
static unsigned failures;
static enum gfx_ctx_api active_api;
static bool list_available;
static struct string_list_elem devices[3];
static struct string_list list;

struct string_list *video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   return list_available && api != GFX_CTX_NONE ? &list : NULL;
}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   return active_api;
}

settings_t *config_get_ptr(void)
{
   return settings_available ? &settings : NULL;
}

static void test_warn(const char *format, ...)
{
   (void)format;
   warnings++;
}

#define RARCH_WARN test_warn
#include "resolver.inc"

#define CHECK(condition) do { if (!(condition)) { \
   fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #condition); \
   failures++; \
} } while (0)

#include "menu.inc"

static int *index_setting(enum gfx_ctx_api api)
{
   switch (api)
   {
      case GFX_CTX_VULKAN_API: return &settings.ints.vulkan_gpu_index;
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API: return &settings.ints.gl_gpu_index;
      case GFX_CTX_DIRECT3D10_API: return &settings.ints.d3d10_gpu_index;
      case GFX_CTX_DIRECT3D11_API: return &settings.ints.d3d11_gpu_index;
      case GFX_CTX_DIRECT3D12_API: return &settings.ints.d3d12_gpu_index;
      case GFX_CTX_METAL_API: return &settings.ints.metal_gpu_index;
      default: return NULL;
   }
}

static char *reset(enum gfx_ctx_api api)
{
   char *saved = video_driver_gpu_name_setting(api);
   active_api = api;
   list_available = true;
   *index_setting(api) = 0;
   settings.flags = 0;
   *saved = '\0';
   devices[0].data = "Default";
   devices[1].data = "GPU A";
   devices[2].data = "GPU B";
   list.elems = devices;
   list.size = 3;
   warnings = 0;
   return saved;
}

static void test_api(enum gfx_ctx_api api)
{
   char *saved = reset(api);
   struct string_list empty;
   int next;

   memset(&empty, 0, sizeof(empty));
   strlcpy(saved, "GPU B", NAME_MAX_LENGTH);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));
   CHECK(warnings == 1);

   /* EGL writes the result back; other callers keep the input. */
   next = (api == GFX_CTX_OPENGL_API || api == GFX_CTX_OPENGL_ES_API)
      ? 2 : 1;
   CHECK(video_driver_gpu_index_resolve(api, next, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));

   saved = reset(api);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 1);
   CHECK(!strcmp(saved, "GPU A"));
   video_driver_gpu_index_select(api, 2);
   CHECK(!strcmp(saved, "GPU B"));
   CHECK(video_driver_gpu_index_resolve(api, 2, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));
   CHECK(warnings == 0);
   CHECK(video_driver_gpu_index_resolve(api, 0, &list) == 0);
   CHECK(!*saved);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 1);
   CHECK(!strcmp(saved, "GPU A"));

   devices[1].data = "GPU B";
   devices[2].data = "GPU A";
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 2);
   CHECK(!strcmp(saved, "GPU A"));

   saved = reset(api);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 1);
   CHECK(video_driver_gpu_index_resolve(api, 2, NULL) == 2);
   CHECK(video_driver_gpu_index_resolve(api, 2, &empty) == 2);
   CHECK(!strcmp(saved, "GPU A"));
   video_driver_gpu_index_select(api, 2);
   CHECK(video_driver_gpu_index_resolve(api, 2, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));

   saved = reset(api);
   strlcpy(saved, "Missing", NAME_MAX_LENGTH);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 0);
   CHECK(!*saved);
   CHECK(warnings == 1);
   CHECK(video_driver_gpu_index_resolve(api, 9, &list) == 0);
   CHECK(!*saved);
   CHECK(warnings == 2);
   CHECK(video_driver_gpu_index_resolve(api, -1, &list) == 0);
   CHECK(!*saved);
   CHECK(video_driver_gpu_index_resolve(api, 2, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));
}

static void test_menu(enum gfx_ctx_api api)
{
   char *saved = reset(api);
   int *index = index_setting(api);
   rarch_setting_t row;

   *index = 1;
   strlcpy(saved, "GPU A", NAME_MAX_LENGTH);
   CHECK(action_right_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 2 && !strcmp(saved, "GPU B"));
   CHECK(settings.flags & SETTINGS_FLG_MODIFIED);
   CHECK(video_driver_gpu_index_resolve(api, *index, &list) == 2);
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 1 && !strcmp(saved, "GPU A"));
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 0 && !*saved);
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 2 && !strcmp(saved, "GPU B"));
   CHECK(action_right_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 0 && !*saved);

   /* Past the list after a device went away */
   *index = 3;
   strlcpy(saved, "GPU B", NAME_MAX_LENGTH);
   CHECK(action_right_video_gpu_index(0, "", true) == 0);
   if (api == GFX_CTX_OPENGL_API || api == GFX_CTX_OPENGL_ES_API)
      CHECK(*index == 0 && !*saved);
   else
   {
      CHECK(*index == 3 && !strcmp(saved, "GPU B"));
      CHECK(video_driver_gpu_index_resolve(api, *index, &list) == 2);
   }
   *index = 5;
   strlcpy(saved, "GPU B", NAME_MAX_LENGTH);
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 4 && !strcmp(saved, "GPU B"));
   CHECK(video_driver_gpu_index_resolve(api, *index, &list) == 2);

   memset(&row, 0, sizeof(row));
   row.enum_idx = MENU_ENUM_LABEL_VIDEO_GPU_INDEX;
   row.value.target.integer = index;
   *index = 2;
   write_setting(&row);
   CHECK(!strcmp(saved, "GPU B"));
   *index = 0;
   write_setting(&row);
   CHECK(!*saved);

   *index = 1;
   strlcpy(saved, "GPU A", NAME_MAX_LENGTH);
   list.size = 0;
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(action_right_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 1 && !strcmp(saved, "GPU A"));
   list_available = false;
   CHECK(action_left_video_gpu_index(0, "", true) == 0);
   CHECK(action_right_video_gpu_index(0, "", true) == 0);
   CHECK(*index == 1 && !strcmp(saved, "GPU A"));
}

static void test_reload(enum gfx_ctx_api api)
{
   char *saved = reset(api);

   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 1);
   strlcpy(saved, "GPU A", NAME_MAX_LENGTH);
   CHECK(video_driver_gpu_index_resolve(api, 2, &list) == 1);
   CHECK(!strcmp(saved, "GPU A"));
   strlcpy(saved, "GPU B", NAME_MAX_LENGTH);
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));
   video_driver_gpu_index_select(api, 1);
   CHECK(!strcmp(saved, "GPU A"));
   CHECK(video_driver_gpu_index_resolve(api, 1, &list) == 1);
}

static void test_shared_gl(void)
{
   char *saved = reset(GFX_CTX_OPENGL_API);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_OPENGL_API, 1, &list) == 1);
   video_driver_gpu_index_select(GFX_CTX_OPENGL_ES_API, 2);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_OPENGL_ES_API, 2, &list) == 2);
   CHECK(!strcmp(saved, "GPU B"));
   devices[1].data = "GPU B";
   devices[2].data = "GPU A";
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_OPENGL_API, 2, &list) == 1);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_OPENGL_ES_API, 1, &list) == 1);
   CHECK(!strcmp(saved, "GPU B"));
}

static void test_api_isolation(void)
{
   char *vk = reset(GFX_CTX_VULKAN_API);
   char *gl = reset(GFX_CTX_OPENGL_API);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_VULKAN_API, 1, &list) == 1);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_OPENGL_API, 2, &list) == 2);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_VULKAN_API, 1, &list) == 1);
   CHECK(!strcmp(vk, "GPU A"));
   CHECK(!strcmp(gl, "GPU B"));
}

int main(void)
{
   static const enum gfx_ctx_api apis[] = {
      GFX_CTX_VULKAN_API, GFX_CTX_OPENGL_API, GFX_CTX_OPENGL_ES_API,
      GFX_CTX_DIRECT3D10_API, GFX_CTX_DIRECT3D11_API,
      GFX_CTX_DIRECT3D12_API, GFX_CTX_METAL_API
   };
   size_t i;

   for (i = 0; i < sizeof(apis) / sizeof(apis[0]); i++)
   {
      test_api(apis[i]);
      test_menu(apis[i]);
      test_reload(apis[i]);
   }
   test_shared_gl();
   test_api_isolation();
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_NONE, 1, &list) == 1);
   video_driver_gpu_index_select(GFX_CTX_NONE, 1);
   settings_available = false;
   video_driver_gpu_index_select(GFX_CTX_VULKAN_API, 2);
   CHECK(video_driver_gpu_index_resolve(GFX_CTX_VULKAN_API, 2, &list) == 2);

   if (failures)
      return 1;
   puts("GPU selection identity: passed");
   return 0;
}

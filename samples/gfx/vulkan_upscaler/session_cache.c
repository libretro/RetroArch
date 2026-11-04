/* The DLSS session cache of gfx/common/vulkan_common.c, built on
 * Win64 only, with a custom driver: the kept session goes before the
 * custom driver's instance is made, and that instance is neither the
 * kept one nor given a key, which vulkan_context_destroy() needs to keep
 * a session. Without one, a matching request reuses the kept session. */
#include <stdio.h>

#ifdef _WIN64
#define HAVE_VULKAN_CUSTOM_DRIVER
#include "../../../gfx/common/vulkan_common.c"
#endif

#ifdef VULKAN_SESSION_CACHE
#define KEPT_INSTANCE ((VkInstance)(uintptr_t)0x1000)
#define KEPT_DEVICE   ((VkDevice)(uintptr_t)0x2000)
#define NEW_INSTANCE  ((VkInstance)(uintptr_t)0x3000)

static unsigned kept_destroyed;

bool video_driver_thread_wrapper_active(void) { return false; }

/* What vulkan_common.c calls of the custom driver glue */
dylib_t vulkan_custom_driver_open(void) { return NULL; }
void vulkan_custom_driver_failed(void) { }
void vulkan_custom_driver_kept(void) { }
size_t vulkan_custom_driver_loader_exts(char *names, size_t stride,
      size_t count, size_t cap) { return count; }
bool vulkan_custom_driver_current(void) { return false; }
void vulkan_custom_driver_started(void) { }
bool vulkan_custom_driver_device(function_t queue_submit) { return true; }
void vulkan_custom_driver_reason(enum vulkan_custom_driver_msg msg) { }

static VKAPI_ATTR VkResult VKAPI_CALL cache_create_instance(
      const VkInstanceCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkInstance *instance)
{
   *instance = NEW_INSTANCE;
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL cache_destroy_instance(
      VkInstance instance, const VkAllocationCallbacks *allocator)
{
   if (instance == KEPT_INSTANCE)
      kept_destroyed++;
}

static VKAPI_ATTR void VKAPI_CALL cache_destroy_device(
      VkDevice device, const VkAllocationCallbacks *allocator)
{
   if (device == KEPT_DEVICE)
      kept_destroyed++;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL cache_icd(
      VkInstance instance, const char *name)
{
   return NULL;
}

/* A session kept by an earlier init with the same request */
static void cache_keep(const VkInstanceCreateInfo *info)
{
   vulkan_session_key_t key   = vulkan_session_instance_key(info);
   kept_dlss_session.instance = KEPT_INSTANCE;
   kept_dlss_session.device   = KEPT_DEVICE;
   kept_dlss_destroy_instance = cache_destroy_instance;
   kept_dlss_destroy_device   = cache_destroy_device;
   vulkan_session_key_store(&kept_dlss_session.session_instance_key, &key);
   kept_destroyed             = 0;
}
#endif

/* With DLSS set and wanted. Returns the failures. */
int session_cache_test(int *checks)
{
   int failures = 0;
#ifdef VULKAN_SESSION_CACHE
   gfx_ctx_vulkan_data_t vk;
   VkApplicationInfo app;
   VkInstanceCreateInfo info;
   VkInstance instance;
#define CACHE_CHECK(cond) \
   do { \
      (*checks)++; \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)
   memset(&vk, 0, sizeof(vk));
   memset(&app, 0, sizeof(app));
   memset(&info, 0, sizeof(info));
   app.sType             = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app.apiVersion        = VK_API_VERSION_1_1;
   info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   info.pApplicationInfo = &app;
   vkCreateInstance      = cache_create_instance;

   cache_keep(&info);
   instance = VK_NULL_HANDLE;
   CACHE_CHECK(vulkan_context_create_instance(&vk, &info, &instance) == VK_SUCCESS);
   CACHE_CHECK(instance == KEPT_INSTANCE);
   CACHE_CHECK(kept_dlss_borrowed_instance == KEPT_INSTANCE);
   CACHE_CHECK(vk.context.session_instance_key != NULL);
   vulkan_session_release(&vk.context);
   vulkan_session_discard();

   /* The same request, without the loader's driver list in its chain,
    * so that only the custom driver tells it apart */
   cache_keep(&info);
   vulkan_custom_icd = cache_icd;
   instance = VK_NULL_HANDLE;
   CACHE_CHECK(vulkan_context_create_instance(&vk, &info, &instance) == VK_SUCCESS);
   CACHE_CHECK(instance == NEW_INSTANCE);
   CACHE_CHECK(kept_dlss_borrowed_instance == VK_NULL_HANDLE);
   CACHE_CHECK(kept_destroyed == 2 && !kept_dlss_session.instance
         && !kept_dlss_session.device);
   CACHE_CHECK(vk.context.session_instance_key == NULL);
   vulkan_custom_icd = NULL;
   vulkan_session_release(&vk.context);
   vulkan_session_discard();
#undef CACHE_CHECK
#else
   (void)checks;
#endif
   return failures;
}

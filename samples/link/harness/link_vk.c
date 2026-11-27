/* The link harness's Vulkan consoles: one instance and one device for
 * each, every entry point the harness calls resolved for that console's
 * own, and each frame copied from the core's image into a mapped buffer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <libretro_vulkan.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>

#include "link_vk.h"

#define LINK_VK_DEVICES  16 /* the physical devices looked at */
#define LINK_VK_FAMILIES 16 /* the queue families of one looked at */
#define LINK_VK_QUEUE    (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
#define LINK_VK_LAYER    "VK_LAYER_KHRONOS_validation"
#define LINK_VK_TIMEOUT  ((uint64_t)120 * 1000000000UL) /* ns */

/* The entry points the harness calls, but for the loader's own */
struct link_vk_entries
{
   PFN_vkDestroyInstance destroy_instance;
   PFN_vkEnumeratePhysicalDevices enumerate_physical_devices;
   PFN_vkGetPhysicalDeviceProperties get_properties;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_families;
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory_properties;
   PFN_vkCreateDevice create_device;
   PFN_vkGetDeviceProcAddr get_device_proc_addr;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   PFN_vkDestroyDevice destroy_device;
   PFN_vkGetDeviceQueue get_queue;
   PFN_vkCreateCommandPool create_command_pool;
   PFN_vkDestroyCommandPool destroy_command_pool;
   PFN_vkAllocateCommandBuffers allocate_command_buffers;
   PFN_vkBeginCommandBuffer begin_command_buffer;
   PFN_vkEndCommandBuffer end_command_buffer;
   PFN_vkCmdPipelineBarrier cmd_pipeline_barrier;
   PFN_vkCmdCopyImageToBuffer cmd_copy_image_to_buffer;
   PFN_vkCreateFence create_fence;
   PFN_vkDestroyFence destroy_fence;
   PFN_vkResetFences reset_fences;
   PFN_vkWaitForFences wait_for_fences;
   PFN_vkQueueSubmit queue_submit;
   PFN_vkCreateBuffer create_buffer;
   PFN_vkDestroyBuffer destroy_buffer;
   PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements;
   PFN_vkAllocateMemory allocate_memory;
   PFN_vkFreeMemory free_memory;
   PFN_vkBindBufferMemory bind_buffer_memory;
   PFN_vkMapMemory map_memory;
   PFN_vkInvalidateMappedMemoryRanges invalidate_mapped_memory;
};

struct link_vk
{
   struct retro_hw_render_interface_vulkan iface;   /* its handle is this */
   struct link_vk_entries f;
   VkPhysicalDeviceProperties properties;
   VkInstance instance;
   VkDebugUtilsMessengerEXT messenger;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   VkCommandPool pool;
   VkCommandBuffer cmd;                /* the readback's */
   VkFence fence;                      /* the last submit's */
   VkBuffer buffer;                    /* the copy's */
   VkDeviceMemory memory;
   VkDeviceSize buffer_size;
   void *mapped;
   const struct retro_vulkan_image *image;  /* set_image's, kept alive by the core */
   const char *missing;                /* the first entry point not found */
   unsigned *errors;
   unsigned index;
   uint32_t family;                    /* the queue's */
   bool coherent;                      /* the buffer's memory needs no invalidation */
   bool pending;                       /* the fence has a submit to wait for */
   bool lost;                          /* the fence did not signal: nothing is destroyed */
};

static struct
{
   dylib_t lib;
   PFN_vkGetInstanceProcAddr get_instance_proc_addr;
   PFN_vkCreateInstance create_instance;
   PFN_vkEnumerateInstanceLayerProperties enumerate_layers;
   bool tried;
} link_vk_now;

bool link_vk_loader(unsigned index)
{
#if defined(_WIN32)
   static const char *const names[] = { "vulkan-1.dll" };
#elif defined(__APPLE__)
   static const char *const names[] = { "libvulkan.dylib", "MoltenVK" };
#else
   static const char *const names[] = { "libvulkan.so.1", "libvulkan.so" };
#endif
   size_t i;
   if (!link_vk_now.tried)
   {
      link_vk_now.tried = true;
      for (i = 0; i < sizeof(names) / sizeof(*names) && !link_vk_now.lib; i++)
         link_vk_now.lib = dylib_load(names[i]);
      if (link_vk_now.lib)
         link_vk_now.get_instance_proc_addr = (PFN_vkGetInstanceProcAddr)dylib_proc(
               link_vk_now.lib, "vkGetInstanceProcAddr");
      if (link_vk_now.get_instance_proc_addr)
      {
         link_vk_now.create_instance = (PFN_vkCreateInstance)
            link_vk_now.get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance");
         link_vk_now.enumerate_layers = (PFN_vkEnumerateInstanceLayerProperties)
            link_vk_now.get_instance_proc_addr(VK_NULL_HANDLE,
                  "vkEnumerateInstanceLayerProperties");
      }
   }
   if (link_vk_now.create_instance && link_vk_now.enumerate_layers)
      return true;
   fprintf(stderr, "link_harness: console %u: no Vulkan loader (%s)\n", index, names[0]);
   return false;
}

/* false, with what failed and the VkResult it gave, if any, on stderr */
static bool link_vk_fail(const link_vk_t *v, const char *what, VkResult result)
{
   fprintf(stderr, "link_harness: console %u: %s", v->index, what);
   if (result != VK_SUCCESS)
      fprintf(stderr, " (VkResult %d)", (int)result);
   fputc('\n', stderr);
   return false;
}

/* false, and why the console has no device */
static bool link_vk_refuse(const link_vk_t *v, const char *why, const char *name,
      VkResult result)
{
   fprintf(stderr, "link_harness: console %u: no Vulkan device: %s%s", v->index, why,
         name ? name : "");
   if (result != VK_SUCCESS)
      fprintf(stderr, " (VkResult %d)", (int)result);
   fputc('\n', stderr);
   return false;
}

/* A call of the interface this harness does not serve */
static void link_vk_misuse(link_vk_t *v, const char *what)
{
   fprintf(stderr, "link_harness: console %u: %s\n", v->index, what);
   (*v->errors)++;
}

/* f, the entry point called name; the first one missing is kept */
static PFN_vkVoidFunction link_vk_found(link_vk_t *v, PFN_vkVoidFunction f, const char *name)
{
   if (!f && !v->missing)
      v->missing = name;
   return f;
}

static PFN_vkVoidFunction link_vk_instance_entry(link_vk_t *v, const char *name)
{
   return link_vk_found(v, link_vk_now.get_instance_proc_addr(v->instance, name), name);
}

static PFN_vkVoidFunction link_vk_device_entry(link_vk_t *v, const char *name)
{
   return link_vk_found(v, v->f.get_device_proc_addr(v->device, name), name);
}

/* An error the validation layer reports, of the core's calls or the
 * harness's */
static VKAPI_ATTR VkBool32 VKAPI_CALL link_vk_validation(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
      const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
   link_vk_t *v = (link_vk_t*)user;
   (void)severity;
   (void)types;
   fprintf(stderr, "link_harness: console %u: validation: [ %s ] %s\n", v->index,
         data->pMessageIdName ? data->pMessageIdName : "", data->pMessage ? data->pMessage : "");
   (*v->errors)++;
   return VK_FALSE;
}

static bool link_vk_has_layer(void)
{
   VkLayerProperties *layers;
   uint32_t count = 0, i;
   bool found     = false;
   VkResult result;
   if (link_vk_now.enumerate_layers(&count, NULL) != VK_SUCCESS || !count
         || !(layers = (VkLayerProperties*)calloc(count, sizeof(*layers))))
      return false;
   result = link_vk_now.enumerate_layers(&count, layers);
   for (i = 0; (result == VK_SUCCESS || result == VK_INCOMPLETE) && i < count && !found; i++)
      found = !strcmp(layers[i].layerName, LINK_VK_LAYER);
   free(layers);
   return found;
}

/* The instance, Vulkan 1.1, with the validation layer and a messenger when
 * the run validates */
static bool link_vk_instance(link_vk_t *v, bool validate)
{
   static const char *const layers[]     = { LINK_VK_LAYER };
   static const char *const extensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
   VkApplicationInfo application;
   VkDebugUtilsMessengerCreateInfoEXT listener;
   VkInstanceCreateInfo info;
   VkResult result;
   memset(&application, 0, sizeof(application));
   application.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application.pApplicationName = "link_harness";
   application.apiVersion       = VK_API_VERSION_1_1;
   memset(&listener, 0, sizeof(listener));
   listener.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   listener.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   listener.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   listener.pfnUserCallback = link_vk_validation;
   listener.pUserData       = v;
   memset(&info, 0, sizeof(info));
   info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   info.pApplicationInfo = &application;
   if (validate)
   {
      if (!link_vk_has_layer())
         return link_vk_refuse(v, "no layer ", LINK_VK_LAYER, VK_SUCCESS);
      /* The listener hears vkCreateInstance and vkDestroyInstance too */
      info.pNext                   = &listener;
      info.enabledLayerCount       = 1;
      info.ppEnabledLayerNames     = layers;
      info.enabledExtensionCount   = 1;
      info.ppEnabledExtensionNames = extensions;
   }
   if ((result = link_vk_now.create_instance(&info, NULL, &v->instance)) != VK_SUCCESS)
   {
      v->instance = VK_NULL_HANDLE;
      return link_vk_refuse(v, "vkCreateInstance failed", NULL, result);
   }
   v->f.destroy_instance = (PFN_vkDestroyInstance)link_vk_instance_entry(v,
         "vkDestroyInstance");
   v->f.enumerate_physical_devices = (PFN_vkEnumeratePhysicalDevices)link_vk_instance_entry(v,
         "vkEnumeratePhysicalDevices");
   v->f.get_properties = (PFN_vkGetPhysicalDeviceProperties)link_vk_instance_entry(v,
         "vkGetPhysicalDeviceProperties");
   v->f.get_queue_families = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
      link_vk_instance_entry(v, "vkGetPhysicalDeviceQueueFamilyProperties");
   v->f.get_memory_properties = (PFN_vkGetPhysicalDeviceMemoryProperties)
      link_vk_instance_entry(v, "vkGetPhysicalDeviceMemoryProperties");
   v->f.create_device = (PFN_vkCreateDevice)link_vk_instance_entry(v, "vkCreateDevice");
   v->f.get_device_proc_addr = (PFN_vkGetDeviceProcAddr)link_vk_instance_entry(v,
         "vkGetDeviceProcAddr");
   if (validate)
   {
      v->f.create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)link_vk_instance_entry(v,
            "vkCreateDebugUtilsMessengerEXT");
      v->f.destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)link_vk_instance_entry(v,
            "vkDestroyDebugUtilsMessengerEXT");
   }
   if (v->missing)
      return link_vk_refuse(v, "no entry point ", v->missing, VK_SUCCESS);
   if (validate && (result = v->f.create_messenger(v->instance, &listener, NULL,
         &v->messenger)) != VK_SUCCESS)
   {
      v->messenger = VK_NULL_HANDLE;
      return link_vk_refuse(v, "vkCreateDebugUtilsMessengerEXT failed", NULL, result);
   }
   return true;
}

/* The first physical device whose name has require in it, else the first,
 * and the first of its queue families that does graphics and compute */
static bool link_vk_gpu(link_vk_t *v, const char *require)
{
   VkPhysicalDevice gpus[LINK_VK_DEVICES];
   VkQueueFamilyProperties families[LINK_VK_FAMILIES];
   uint32_t count  = LINK_VK_DEVICES;
   VkResult result = v->f.enumerate_physical_devices(v->instance, &count, gpus);
   uint32_t i;
   if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || !count)
      return link_vk_refuse(v, "no physical device", NULL, result);
   v->gpu = gpus[0];
   for (i = 0; require && i < count; i++)
   {
      v->f.get_properties(gpus[i], &v->properties);
      if (strstr(v->properties.deviceName, require))
      {
         v->gpu = gpus[i];
         break;
      }
   }
   v->f.get_properties(v->gpu, &v->properties);
   if (require && !strstr(v->properties.deviceName, require))
   {
      fprintf(stderr, "link_harness: console %u: renderer \"%s\" is not %s\n", v->index,
            v->properties.deviceName, require);
      return false;
   }
   count = LINK_VK_FAMILIES;
   v->f.get_queue_families(v->gpu, &count, families);
   for (i = 0; i < count; i++)
      if ((families[i].queueFlags & LINK_VK_QUEUE) == LINK_VK_QUEUE && families[i].queueCount)
      {
         v->family = i;
         return true;
      }
   return link_vk_refuse(v, "no queue family does graphics and compute", NULL, VK_SUCCESS);
}

/* The device, with one queue of that family and nothing else asked for */
static bool link_vk_device(link_vk_t *v)
{
   static const float priority = 1.0f;
   VkDeviceQueueCreateInfo queue;
   VkDeviceCreateInfo info;
   VkResult result;
   memset(&queue, 0, sizeof(queue));
   queue.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.queueFamilyIndex = v->family;
   queue.queueCount       = 1;
   queue.pQueuePriorities = &priority;
   memset(&info, 0, sizeof(info));
   info.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   info.queueCreateInfoCount = 1;
   info.pQueueCreateInfos    = &queue;
   if ((result = v->f.create_device(v->gpu, &info, NULL, &v->device)) != VK_SUCCESS)
   {
      v->device = VK_NULL_HANDLE;
      return link_vk_refuse(v, "vkCreateDevice failed", NULL, result);
   }
   v->f.destroy_device = (PFN_vkDestroyDevice)link_vk_device_entry(v, "vkDestroyDevice");
   v->f.get_queue      = (PFN_vkGetDeviceQueue)link_vk_device_entry(v, "vkGetDeviceQueue");
   v->f.create_command_pool = (PFN_vkCreateCommandPool)link_vk_device_entry(v,
         "vkCreateCommandPool");
   v->f.destroy_command_pool = (PFN_vkDestroyCommandPool)link_vk_device_entry(v,
         "vkDestroyCommandPool");
   v->f.allocate_command_buffers = (PFN_vkAllocateCommandBuffers)link_vk_device_entry(v,
         "vkAllocateCommandBuffers");
   v->f.begin_command_buffer = (PFN_vkBeginCommandBuffer)link_vk_device_entry(v,
         "vkBeginCommandBuffer");
   v->f.end_command_buffer = (PFN_vkEndCommandBuffer)link_vk_device_entry(v,
         "vkEndCommandBuffer");
   v->f.cmd_pipeline_barrier = (PFN_vkCmdPipelineBarrier)link_vk_device_entry(v,
         "vkCmdPipelineBarrier");
   v->f.cmd_copy_image_to_buffer = (PFN_vkCmdCopyImageToBuffer)link_vk_device_entry(v,
         "vkCmdCopyImageToBuffer");
   v->f.create_fence    = (PFN_vkCreateFence)link_vk_device_entry(v, "vkCreateFence");
   v->f.destroy_fence   = (PFN_vkDestroyFence)link_vk_device_entry(v, "vkDestroyFence");
   v->f.reset_fences    = (PFN_vkResetFences)link_vk_device_entry(v, "vkResetFences");
   v->f.wait_for_fences = (PFN_vkWaitForFences)link_vk_device_entry(v, "vkWaitForFences");
   v->f.queue_submit    = (PFN_vkQueueSubmit)link_vk_device_entry(v, "vkQueueSubmit");
   v->f.create_buffer   = (PFN_vkCreateBuffer)link_vk_device_entry(v, "vkCreateBuffer");
   v->f.destroy_buffer  = (PFN_vkDestroyBuffer)link_vk_device_entry(v, "vkDestroyBuffer");
   v->f.get_buffer_memory_requirements = (PFN_vkGetBufferMemoryRequirements)
      link_vk_device_entry(v, "vkGetBufferMemoryRequirements");
   v->f.allocate_memory = (PFN_vkAllocateMemory)link_vk_device_entry(v, "vkAllocateMemory");
   v->f.free_memory     = (PFN_vkFreeMemory)link_vk_device_entry(v, "vkFreeMemory");
   v->f.bind_buffer_memory = (PFN_vkBindBufferMemory)link_vk_device_entry(v,
         "vkBindBufferMemory");
   v->f.map_memory = (PFN_vkMapMemory)link_vk_device_entry(v, "vkMapMemory");
   v->f.invalidate_mapped_memory = (PFN_vkInvalidateMappedMemoryRanges)link_vk_device_entry(v,
         "vkInvalidateMappedMemoryRanges");
   if (v->missing)
      return link_vk_refuse(v, "no entry point ", v->missing, VK_SUCCESS);
   v->f.get_queue(v->device, v->family, 0, &v->queue);
   return true;
}

/* The command buffer the readback records and the fence of its submits */
static bool link_vk_readback(link_vk_t *v)
{
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo buffer;
   VkFenceCreateInfo fence;
   VkResult result;
   memset(&pool, 0, sizeof(pool));
   pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = v->family;
   if ((result = v->f.create_command_pool(v->device, &pool, NULL, &v->pool)) != VK_SUCCESS)
   {
      v->pool = VK_NULL_HANDLE;
      return link_vk_refuse(v, "vkCreateCommandPool failed", NULL, result);
   }
   memset(&buffer, 0, sizeof(buffer));
   buffer.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   buffer.commandPool        = v->pool;
   buffer.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   buffer.commandBufferCount = 1;
   if ((result = v->f.allocate_command_buffers(v->device, &buffer, &v->cmd)) != VK_SUCCESS)
      return link_vk_refuse(v, "vkAllocateCommandBuffers failed", NULL, result);
   memset(&fence, 0, sizeof(fence));
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if ((result = v->f.create_fence(v->device, &fence, NULL, &v->fence)) != VK_SUCCESS)
   {
      v->fence = VK_NULL_HANDLE;
      return link_vk_refuse(v, "vkCreateFence failed", NULL, result);
   }
   return true;
}

/* The core's calls of the interface */

static void link_vk_set_image(void *handle, const struct retro_vulkan_image *image,
      uint32_t num_semaphores, const VkSemaphore *semaphores, uint32_t src_queue_family)
{
   link_vk_t *v = (link_vk_t*)handle;
   (void)semaphores;
   v->image = image;
   if (num_semaphores)
      link_vk_misuse(v, "set_image with semaphores is not served");
   if (src_queue_family != VK_QUEUE_FAMILY_IGNORED && src_queue_family != v->family)
      link_vk_misuse(v, "set_image's src_queue_family is another family, which is not served");
}

static uint32_t link_vk_sync_index(void *handle)
{
   (void)handle;
   return 0;
}

static uint32_t link_vk_sync_index_mask(void *handle)
{
   (void)handle;
   return 1;
}

static void link_vk_set_command_buffers(void *handle, uint32_t num_cmd,
      const VkCommandBuffer *cmd)
{
   (void)num_cmd;
   (void)cmd;
   link_vk_misuse((link_vk_t*)handle, "set_command_buffers is not served");
}

static void link_vk_wait_sync_index(void *handle)
{
   link_vk_wait((link_vk_t*)handle);
}

/* lock_queue and unlock_queue: the harness submits only inside the video
 * callback, on the thread that runs the core */
static void link_vk_queue_held(void *handle)
{
   (void)handle;
}

static void link_vk_set_signal_semaphore(void *handle, VkSemaphore semaphore)
{
   (void)semaphore;
   link_vk_misuse((link_vk_t*)handle, "set_signal_semaphore is not served");
}

link_vk_t *link_vk_new(unsigned index, const char *require, bool validate, unsigned *errors)
{
   link_vk_t *v = (link_vk_t*)calloc(1, sizeof(*v));
   if (!v)
   {
      fprintf(stderr, "link_harness: console %u: no Vulkan device: out of memory\n", index);
      return NULL;
   }
   v->index  = index;
   v->errors = errors;
   if (     !link_vk_instance(v, validate)
         || !link_vk_gpu(v, require)
         || !link_vk_device(v)
         || !link_vk_readback(v))
   {
      link_vk_free(v);
      return NULL;
   }
   v->iface.interface_type         = RETRO_HW_RENDER_INTERFACE_VULKAN;
   v->iface.interface_version      = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
   v->iface.handle                 = v;
   v->iface.instance               = v->instance;
   v->iface.gpu                    = v->gpu;
   v->iface.device                 = v->device;
   v->iface.get_device_proc_addr   = v->f.get_device_proc_addr;
   v->iface.get_instance_proc_addr = link_vk_now.get_instance_proc_addr;
   v->iface.queue                  = v->queue;
   v->iface.queue_index            = v->family;
   v->iface.set_image              = link_vk_set_image;
   v->iface.get_sync_index         = link_vk_sync_index;
   v->iface.get_sync_index_mask    = link_vk_sync_index_mask;
   v->iface.set_command_buffers    = link_vk_set_command_buffers;
   v->iface.wait_sync_index        = link_vk_wait_sync_index;
   v->iface.lock_queue             = link_vk_queue_held;
   v->iface.unlock_queue           = link_vk_queue_held;
   v->iface.set_signal_semaphore   = link_vk_set_signal_semaphore;
   return v;
}

const struct retro_hw_render_interface_vulkan *link_vk_interface(link_vk_t *v)
{
   return &v->iface;
}

/* count command buffers, or none for a submit that only signals, submitted
 * with the fence */
static bool link_vk_submit(link_vk_t *v, const VkCommandBuffer *cmd, uint32_t count)
{
   VkSubmitInfo submit;
   VkResult result;
   memset(&submit, 0, sizeof(submit));
   submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = count;
   submit.pCommandBuffers    = cmd;
   if (     (result = v->f.reset_fences(v->device, 1, &v->fence)) != VK_SUCCESS
         || (result = v->f.queue_submit(v->queue, count ? 1 : 0, &submit, v->fence))
            != VK_SUCCESS)
      return link_vk_fail(v, "a submit failed", result);
   v->pending = true;
   return true;
}

/* The last submit, and all submitted on the queue before it, finished.
 * false: the fence did not signal, said on stderr, and the console is
 * lost. */
static bool link_vk_settle(link_vk_t *v)
{
   VkResult result;
   if (v->lost)
      return false;
   if (!v->pending)
      return true;
   /* The fence is the event that ends the wait: a test tool has the frame
    * finished before its core runs on. The timeout is a watchdog only. */
   result     = v->f.wait_for_fences(v->device, 1, &v->fence, VK_TRUE, LINK_VK_TIMEOUT);
   v->pending = false;
   if (result == VK_SUCCESS)
      return true;
   v->lost = true;
   return link_vk_fail(v, "the frame's fence did not signal", result);
}

static void link_vk_free_buffer(link_vk_t *v)
{
   v->f.destroy_buffer(v->device, v->buffer, NULL);
   v->f.free_memory(v->device, v->memory, NULL);
   v->buffer      = VK_NULL_HANDLE;
   v->memory      = VK_NULL_HANDLE;
   v->mapped      = NULL;
   v->buffer_size = 0;
}

/* The buffer the copy fills, of size bytes at least, mapped in memory the
 * host sees, cached where there is such */
static bool link_vk_buffer(link_vk_t *v, VkDeviceSize size)
{
   VkPhysicalDeviceMemoryProperties memory;
   VkBufferCreateInfo info;
   VkMemoryRequirements needs;
   VkMemoryAllocateInfo allocate;
   VkMemoryPropertyFlags flags = 0;
   uint32_t type, chosen;
   VkResult result;
   if (size <= v->buffer_size)
      return true;
   link_vk_free_buffer(v);
   memset(&info, 0, sizeof(info));
   info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.size        = size;
   info.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if ((result = v->f.create_buffer(v->device, &info, NULL, &v->buffer)) != VK_SUCCESS)
   {
      v->buffer = VK_NULL_HANDLE;
      return link_vk_fail(v, "vkCreateBuffer failed", result);
   }
   v->f.get_buffer_memory_requirements(v->device, v->buffer, &needs);
   v->f.get_memory_properties(v->gpu, &memory);
   chosen = memory.memoryTypeCount;
   for (type = 0; type < memory.memoryTypeCount; type++)
   {
      VkMemoryPropertyFlags has = memory.memoryTypes[type].propertyFlags;
      if (     !(needs.memoryTypeBits & (1u << type))
            || !(has & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            || (chosen < memory.memoryTypeCount && !(has & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)))
         continue;
      chosen = type;
      flags  = has;
      if (has & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
         break;
   }
   if (chosen == memory.memoryTypeCount)
      return link_vk_fail(v, "no memory the host sees for its readback", VK_SUCCESS);
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.allocationSize  = needs.size;
   allocate.memoryTypeIndex = chosen;
   if ((result = v->f.allocate_memory(v->device, &allocate, NULL, &v->memory)) != VK_SUCCESS)
   {
      v->memory = VK_NULL_HANDLE;
      return link_vk_fail(v, "vkAllocateMemory failed", result);
   }
   if (     (result = v->f.bind_buffer_memory(v->device, v->buffer, v->memory, 0)) != VK_SUCCESS
         || (result = v->f.map_memory(v->device, v->memory, 0, VK_WHOLE_SIZE, 0, &v->mapped))
            != VK_SUCCESS)
   {
      v->mapped = NULL;
      return link_vk_fail(v, "its readback's memory cannot be mapped", result);
   }
   v->coherent    = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
   v->buffer_size = size;
   return true;
}

/* The frame's copy recorded and submitted: width x height of the image's
 * first mip level and array layer into the buffer, the image a transfer
 * source for it and put back in its layout after. A GENERAL image is read
 * in place: libretro_vulkan.h allows the frontend no transition of it. */
static bool link_vk_copy(link_vk_t *v, const struct retro_vulkan_image *image,
      unsigned width, unsigned height)
{
   const VkImageSubresourceRange *range = &image->create_info.subresourceRange;
   VkImageLayout source                 = image->image_layout == VK_IMAGE_LAYOUT_GENERAL
      ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier barrier;
   VkMemoryBarrier host;
   VkBufferImageCopy region;
   VkResult result;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if ((result = v->f.begin_command_buffer(v->cmd, &begin)) != VK_SUCCESS)
      return link_vk_fail(v, "vkBeginCommandBuffer failed", result);
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
   barrier.oldLayout                       = image->image_layout;
   barrier.newLayout                       = source;
   barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                           = image->create_info.image;
   barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel   = range->baseMipLevel;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.baseArrayLayer = range->baseArrayLayer;
   barrier.subresourceRange.layerCount     = 1;
   /* After the core's writes, or after the barrier of its that ends them
    * at the fragment shader, as libretro_vulkan.h's example does */
   v->f.cmd_pipeline_barrier(v->cmd,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
   memset(&region, 0, sizeof(region));
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = range->baseMipLevel;
   region.imageSubresource.baseArrayLayer = range->baseArrayLayer;
   region.imageSubresource.layerCount     = 1;
   region.imageExtent.width               = width;
   region.imageExtent.height              = height;
   region.imageExtent.depth               = 1;
   v->f.cmd_copy_image_to_buffer(v->cmd, barrier.image, source, v->buffer, 1, &region);
   barrier.srcAccessMask = 0;
   barrier.dstAccessMask = 0;
   barrier.oldLayout     = source;
   barrier.newLayout     = image->image_layout;
   memset(&host, 0, sizeof(host));
   host.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   /* The image back before the core's next frame starts at the colour
    * attachment stage, as libretro_vulkan.h has the frontend do, and the
    * copy seen by the host */
   v->f.cmd_pipeline_barrier(v->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
         0, 1, &host, 0, NULL, 1, &barrier);
   if ((result = v->f.end_command_buffer(v->cmd)) != VK_SUCCESS)
      return link_vk_fail(v, "vkEndCommandBuffer failed", result);
   return link_vk_submit(v, &v->cmd, 1);
}

bool link_vk_read(link_vk_t *v, uint32_t *argb, unsigned width, unsigned height)
{
   const struct retro_vulkan_image *image = v->image;
   size_t size                            = (size_t)width * height;
   const unsigned char *bytes;
   VkMappedMemoryRange range;
   VkResult result;
   size_t i;
   if (v->lost)
      return false;
   if (!image)
      return link_vk_fail(v, "a frame with no set_image", VK_SUCCESS);
   if (image->create_info.format != VK_FORMAT_R8G8B8A8_UNORM)
   {
      fprintf(stderr, "link_harness: console %u: an image of format %d is not read\n",
            v->index, (int)image->create_info.format);
      return false;
   }
   if (!size)
   {
      fprintf(stderr, "link_harness: console %u: a frame of %ux%u is not read\n", v->index,
            width, height);
      return false;
   }
   if (     !link_vk_settle(v)
         || !link_vk_buffer(v, (VkDeviceSize)size * 4)
         || !link_vk_copy(v, image, width, height)
         || !link_vk_settle(v))
      return false;
   if (!v->coherent)
   {
      memset(&range, 0, sizeof(range));
      range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      range.memory = v->memory;
      range.size   = VK_WHOLE_SIZE;
      if ((result = v->f.invalidate_mapped_memory(v->device, 1, &range)) != VK_SUCCESS)
         return link_vk_fail(v, "its readback's memory cannot be read", result);
   }
   bytes = (const unsigned char*)v->mapped;
   for (i = 0; i < size; i++, bytes += 4)
      argb[i] = ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) | bytes[2];
   return true;
}

void link_vk_dupe(link_vk_t *v)
{
   if (!v->lost && (!link_vk_settle(v) || !link_vk_submit(v, NULL, 0)))
      (*v->errors)++;
}

void link_vk_wait(link_vk_t *v)
{
   if (!v->lost && !link_vk_settle(v))
      (*v->errors)++;
}

void link_vk_describe(const link_vk_t *v, char *out, size_t len)
{
   char api[48];
   uint32_t version = v->properties.apiVersion;
   sprintf(api, " api %u.%u.%u\n", (unsigned)VK_API_VERSION_MAJOR(version),
         (unsigned)VK_API_VERSION_MINOR(version), (unsigned)VK_API_VERSION_PATCH(version));
   strlcpy(out, "vulkan ", len);
   strlcat(out, v->properties.deviceName, len);
   strlcat(out, api, len);
}

void link_vk_free(link_vk_t *v)
{
   if (!v)
      return;
   /* Every submit on the queue finished, the core's in context_destroy and
    * retro_unload_game too, before anything goes */
   if (     v->fence && !v->lost
         && (!link_vk_settle(v) || !link_vk_submit(v, NULL, 0) || !link_vk_settle(v)))
      (*v->errors)++;
   /* A queue whose fence did not signal may be at work still */
   if (v->lost)
   {
      free(v);
      return;
   }
   if (v->device && !v->missing)
   {
      link_vk_free_buffer(v);
      v->f.destroy_fence(v->device, v->fence, NULL);
      v->f.destroy_command_pool(v->device, v->pool, NULL);
   }
   if (v->device && v->f.destroy_device)
      v->f.destroy_device(v->device, NULL);
   if (v->messenger)
      v->f.destroy_messenger(v->instance, v->messenger, NULL);
   if (v->instance && v->f.destroy_instance)
      v->f.destroy_instance(v->instance, NULL);
   free(v);
}

/* The link test core's two-part picture in Vulkan: cleared rectangles in
 * a render pass, every entry point from the frontend's interface, so no
 * Vulkan library and no shader */

#include <string.h>

#include <libretro.h>
#include <libretro_vulkan.h>

#include "link_test_render.h"

#define LT_VK_FORMAT   VK_FORMAT_R8G8B8A8_UNORM
#define LT_VK_QUEUE    (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
#define LT_VK_FAMILIES 16 /* the queue families of a device looked at */

/* link_test_vk_handoff */
enum lt_vk_handoff
{
   LT_VK_BARRIER = 0, /* part 1's command buffer ends in a barrier */
   LT_VK_SEMAPHORE,   /* and semaphores pass the image both ways */
   LT_VK_COMMANDS     /* part 1's command buffer is the frontend's to submit */
};

/* The entry points the core calls: the device's, but for the first */
struct lt_vk_entries
{
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory_properties;
   PFN_vkCreateImage create_image;
   PFN_vkDestroyImage destroy_image;
   PFN_vkGetImageMemoryRequirements get_image_memory_requirements;
   PFN_vkAllocateMemory allocate_memory;
   PFN_vkFreeMemory free_memory;
   PFN_vkBindImageMemory bind_image_memory;
   PFN_vkCreateImageView create_image_view;
   PFN_vkDestroyImageView destroy_image_view;
   PFN_vkCreateRenderPass create_render_pass;
   PFN_vkDestroyRenderPass destroy_render_pass;
   PFN_vkCreateFramebuffer create_framebuffer;
   PFN_vkDestroyFramebuffer destroy_framebuffer;
   PFN_vkCreateCommandPool create_command_pool;
   PFN_vkDestroyCommandPool destroy_command_pool;
   PFN_vkResetCommandPool reset_command_pool;
   PFN_vkAllocateCommandBuffers allocate_command_buffers;
   PFN_vkBeginCommandBuffer begin_command_buffer;
   PFN_vkEndCommandBuffer end_command_buffer;
   PFN_vkCmdPipelineBarrier cmd_pipeline_barrier;
   PFN_vkCmdBeginRenderPass cmd_begin_render_pass;
   PFN_vkCmdEndRenderPass cmd_end_render_pass;
   PFN_vkCmdClearAttachments cmd_clear_attachments;
   PFN_vkQueueSubmit queue_submit;
   PFN_vkCreateSemaphore create_semaphore;
   PFN_vkDestroySemaphore destroy_semaphore;
};

static struct
{
   const struct lt_render_host *host;
   struct retro_hw_render_callback hw;
   struct retro_hw_render_context_negotiation_interface_vulkan negotiation;
   const struct retro_hw_render_interface_vulkan *vk; /* context_reset's */
   struct lt_vk_entries f;
   VkImage image;
   VkDeviceMemory memory;
   VkImageView view;
   VkFramebuffer framebuffer;
   VkRenderPass pass;
   VkCommandPool pool;
   VkCommandBuffer cmd[2];          /* a part's */
   VkSemaphore drawn;               /* semaphore: part 1 signals it, set_image passes it */
   VkSemaphore reusable;            /* semaphore: the frontend signals it, part 0 waits */
   struct retro_vulkan_image shown; /* what set_image passes */
   VkImageLayout layout;            /* the image's, after what was last recorded */
   unsigned image_width, image_height;
   unsigned width, height;          /* the picture's */
   enum lt_vk_handoff handoff;
   bool entries;                    /* every entry point found */
   bool ready;                      /* every object made */
   bool waits;                      /* part 0 waits for reusable */
   bool drawing;                    /* this frame's picture was begun */
   bool said_no_device;             /* logged, or a failed regrow was; context_reset
                                       clears it */
} lt_vk;

static const VkApplicationInfo lt_vk_application = {
   VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "link test", 0, NULL, 0, VK_API_VERSION_1_1
};

/* False, with what failed as an error, unless result is VK_SUCCESS */
static bool lt_vk_ok(VkResult result, const char *what)
{
   if (result == VK_SUCCESS)
      return true;
   lt_vk.host->fail(what, (uint64_t)result, 0);
   return false;
}

/* f, the entry point called name; the first one missing is an error */
static PFN_vkVoidFunction lt_vk_found(PFN_vkVoidFunction f, const char *name)
{
   char line[64];
   if (f || !lt_vk.entries)
      return f;
   strcpy(line, "no entry point ");
   strncat(line, name, sizeof(line) - 1 - strlen(line));
   lt_vk.host->fail(line, 0, 0);
   lt_vk.entries = false;
   return NULL;
}

static PFN_vkVoidFunction lt_vk_entry(const char *name)
{
   return lt_vk_found(lt_vk.vk->get_device_proc_addr(lt_vk.vk->device, name), name);
}

static bool lt_vk_find_entries(void)
{
   const struct retro_hw_render_interface_vulkan *vk = lt_vk.vk;
   struct lt_vk_entries *e = &lt_vk.f;
   lt_vk.entries = true;
   e->get_memory_properties = (PFN_vkGetPhysicalDeviceMemoryProperties)lt_vk_found(
         vk->get_instance_proc_addr(vk->instance, "vkGetPhysicalDeviceMemoryProperties"),
         "vkGetPhysicalDeviceMemoryProperties");
   e->create_image                  = (PFN_vkCreateImage)lt_vk_entry("vkCreateImage");
   e->destroy_image                 = (PFN_vkDestroyImage)lt_vk_entry("vkDestroyImage");
   e->get_image_memory_requirements = (PFN_vkGetImageMemoryRequirements)lt_vk_entry(
         "vkGetImageMemoryRequirements");
   e->allocate_memory               = (PFN_vkAllocateMemory)lt_vk_entry("vkAllocateMemory");
   e->free_memory                   = (PFN_vkFreeMemory)lt_vk_entry("vkFreeMemory");
   e->bind_image_memory             = (PFN_vkBindImageMemory)lt_vk_entry("vkBindImageMemory");
   e->create_image_view             = (PFN_vkCreateImageView)lt_vk_entry("vkCreateImageView");
   e->destroy_image_view            = (PFN_vkDestroyImageView)lt_vk_entry("vkDestroyImageView");
   e->create_render_pass            = (PFN_vkCreateRenderPass)lt_vk_entry("vkCreateRenderPass");
   e->destroy_render_pass           = (PFN_vkDestroyRenderPass)lt_vk_entry("vkDestroyRenderPass");
   e->create_framebuffer            = (PFN_vkCreateFramebuffer)lt_vk_entry("vkCreateFramebuffer");
   e->destroy_framebuffer           = (PFN_vkDestroyFramebuffer)lt_vk_entry("vkDestroyFramebuffer");
   e->create_command_pool           = (PFN_vkCreateCommandPool)lt_vk_entry("vkCreateCommandPool");
   e->destroy_command_pool          = (PFN_vkDestroyCommandPool)lt_vk_entry("vkDestroyCommandPool");
   e->reset_command_pool            = (PFN_vkResetCommandPool)lt_vk_entry("vkResetCommandPool");
   e->allocate_command_buffers      = (PFN_vkAllocateCommandBuffers)lt_vk_entry(
         "vkAllocateCommandBuffers");
   e->begin_command_buffer          = (PFN_vkBeginCommandBuffer)lt_vk_entry("vkBeginCommandBuffer");
   e->end_command_buffer            = (PFN_vkEndCommandBuffer)lt_vk_entry("vkEndCommandBuffer");
   e->cmd_pipeline_barrier          = (PFN_vkCmdPipelineBarrier)lt_vk_entry("vkCmdPipelineBarrier");
   e->cmd_begin_render_pass         = (PFN_vkCmdBeginRenderPass)lt_vk_entry("vkCmdBeginRenderPass");
   e->cmd_end_render_pass           = (PFN_vkCmdEndRenderPass)lt_vk_entry("vkCmdEndRenderPass");
   e->cmd_clear_attachments         = (PFN_vkCmdClearAttachments)lt_vk_entry(
         "vkCmdClearAttachments");
   e->queue_submit                  = (PFN_vkQueueSubmit)lt_vk_entry("vkQueueSubmit");
   e->create_semaphore              = (PFN_vkCreateSemaphore)lt_vk_entry("vkCreateSemaphore");
   e->destroy_semaphore             = (PFN_vkDestroySemaphore)lt_vk_entry("vkDestroySemaphore");
   return lt_vk.entries;
}

static const VkApplicationInfo *lt_vk_get_application_info(void)
{
   return &lt_vk_application;
}

/* A device with one queue that does graphics and compute, on gpu or the
 * first one when gpu is none, made through wrapper when there is one.
 * info has the frontend's extensions, layers and features. */
static bool lt_vk_device(struct retro_vulkan_context *context, VkInstance instance,
      VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      VkDeviceCreateInfo *info, retro_vulkan_create_device_wrapper_t wrapper, void *opaque)
{
   static const float priority = 1.0f;
   PFN_vkEnumeratePhysicalDevices enumerate = (PFN_vkEnumeratePhysicalDevices)
      get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices");
   PFN_vkGetPhysicalDeviceQueueFamilyProperties get_families =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
      get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
   PFN_vkCreateDevice create = (PFN_vkCreateDevice)
      get_instance_proc_addr(instance, "vkCreateDevice");
   PFN_vkGetDeviceQueue get_queue = (PFN_vkGetDeviceQueue)
      get_instance_proc_addr(instance, "vkGetDeviceQueue");
   VkQueueFamilyProperties families[LT_VK_FAMILIES];
   VkDeviceQueueCreateInfo queue;
   VkDevice device = NULL;
   uint32_t count  = 1;
   uint32_t family;
   if (!enumerate || !get_families || !create || !get_queue)
   {
      lt_vk.host->fail("create_device: an instance entry point missing", 0, 0);
      return false;
   }
   if (!gpu && (enumerate(instance, &count, &gpu) < 0 || !gpu))
   {
      lt_vk.host->fail("create_device: no physical device", 0, 0);
      return false;
   }
   count = LT_VK_FAMILIES;
   get_families(gpu, &count, families);
   for (family = 0; family < count; family++)
      if ((families[family].queueFlags & LT_VK_QUEUE) == LT_VK_QUEUE)
         break;
   if (family == count)
   {
      lt_vk.host->fail("create_device: no queue family does graphics and compute", count, 0);
      return false;
   }
   memset(&queue, 0, sizeof(queue));
   queue.sType                = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.queueFamilyIndex     = family;
   queue.queueCount           = 1;
   queue.pQueuePriorities     = &priority;
   info->sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   info->queueCreateInfoCount = 1;
   info->pQueueCreateInfos    = &queue;
   if (!wrapper)
   {
      if (!lt_vk_ok(create(gpu, info, NULL, &device), "create_device: vkCreateDevice failed"))
         return false;
   }
   else if (!(device = wrapper(gpu, opaque, info)))
   {
      lt_vk.host->fail("create_device2: the frontend's wrapper made no device", 0, 0);
      return false;
   }
   get_queue(device, family, 0, &context->queue);
   context->gpu                             = gpu;
   context->device                          = device;
   context->queue_family_index              = family;
   context->presentation_queue              = context->queue;
   context->presentation_queue_family_index = family;
   return true;
}

/* Negotiation v1, for a frontend that passes no surface */
static bool lt_vk_create_device(struct retro_vulkan_context *context, VkInstance instance,
      VkPhysicalDevice gpu, VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const char **extensions, unsigned extension_count, const char **layers, unsigned layer_count,
      const VkPhysicalDeviceFeatures *features)
{
   VkDeviceCreateInfo info;
   (void)surface;
   memset(&info, 0, sizeof(info));
   info.enabledExtensionCount   = extension_count;
   info.ppEnabledExtensionNames = extensions;
   info.enabledLayerCount       = layer_count;
   info.ppEnabledLayerNames     = layers;
   info.pEnabledFeatures        = features;
   return lt_vk_device(context, instance, gpu, get_instance_proc_addr, &info, NULL, NULL);
}

/* Negotiation v2: the instance and the device through the frontend's
 * wrappers, for a frontend that passes no surface */
static VkInstance lt_vk_create_instance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const VkApplicationInfo *app, retro_vulkan_create_instance_wrapper_t wrapper, void *opaque)
{
   VkInstanceCreateInfo info;
   (void)get_instance_proc_addr;
   memset(&info, 0, sizeof(info));
   info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   info.pApplicationInfo = app;
   return wrapper(opaque, &info);
}

static bool lt_vk_create_device2(struct retro_vulkan_context *context, VkInstance instance,
      VkPhysicalDevice gpu, VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      retro_vulkan_create_device_wrapper_t wrapper, void *opaque)
{
   VkDeviceCreateInfo info;
   (void)surface;
   memset(&info, 0, sizeof(info));
   return lt_vk_device(context, instance, gpu, get_instance_proc_addr, &info, wrapper, opaque);
}

/* The device is the frontend's, and the core made nothing else with it */
static void lt_vk_destroy_device(void)
{
}

static void lt_vk_free_image(void)
{
   VkDevice device = lt_vk.vk->device;
   lt_vk.f.destroy_framebuffer(device, lt_vk.framebuffer, NULL);
   lt_vk.f.destroy_image_view(device, lt_vk.view, NULL);
   lt_vk.f.destroy_image(device, lt_vk.image, NULL);
   lt_vk.f.free_memory(device, lt_vk.memory, NULL);
   lt_vk.framebuffer = VK_NULL_HANDLE;
   lt_vk.view        = VK_NULL_HANDLE;
   lt_vk.image       = VK_NULL_HANDLE;
   lt_vk.memory      = VK_NULL_HANDLE;
}

/* The image, its memory, view and framebuffer, at width x height in place
 * of those before; false: an error, and no frame is drawn */
static bool lt_vk_image(unsigned width, unsigned height)
{
   VkDevice device             = lt_vk.vk->device;
   VkImageViewCreateInfo *view = &lt_vk.shown.create_info;
   VkImageCreateInfo image;
   VkMemoryRequirements needs;
   VkPhysicalDeviceMemoryProperties memory;
   VkMemoryAllocateInfo allocate;
   VkFramebufferCreateInfo framebuffer;
   uint32_t type;
   lt_vk_free_image();
   lt_vk.ready = false;
   /* As libretro_vulkan.h asks of an image for set_image */
   memset(&image, 0, sizeof(image));
   image.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
   image.imageType     = VK_IMAGE_TYPE_2D;
   image.format        = LT_VK_FORMAT;
   image.extent.width  = width;
   image.extent.height = height;
   image.extent.depth  = 1;
   image.mipLevels     = 1;
   image.arrayLayers   = 1;
   image.samples       = VK_SAMPLE_COUNT_1_BIT;
   image.tiling        = VK_IMAGE_TILING_OPTIMAL;
   image.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT;
   image.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (!lt_vk_ok(lt_vk.f.create_image(device, &image, NULL, &lt_vk.image), "vkCreateImage failed"))
      return false;
   lt_vk.f.get_image_memory_requirements(device, lt_vk.image, &needs);
   lt_vk.f.get_memory_properties(lt_vk.vk->gpu, &memory);
   for (type = 0; type < memory.memoryTypeCount; type++)
      if (     (needs.memoryTypeBits & (1u << type))
            && (memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
         break;
   if (type == memory.memoryTypeCount)
   {
      lt_vk.host->fail("no device-local memory for the image", needs.memoryTypeBits, 0);
      return false;
   }
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.allocationSize  = needs.size;
   allocate.memoryTypeIndex = type;
   if (   !lt_vk_ok(lt_vk.f.allocate_memory(device, &allocate, NULL, &lt_vk.memory),
            "vkAllocateMemory failed")
       || !lt_vk_ok(lt_vk.f.bind_image_memory(device, lt_vk.image, lt_vk.memory, 0),
            "vkBindImageMemory failed"))
      return false;
   memset(view, 0, sizeof(*view));
   view->sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view->image                       = lt_vk.image;
   view->viewType                    = VK_IMAGE_VIEW_TYPE_2D;
   view->format                      = LT_VK_FORMAT;
   view->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view->subresourceRange.levelCount = 1;
   view->subresourceRange.layerCount = 1;
   if (!lt_vk_ok(lt_vk.f.create_image_view(device, view, NULL, &lt_vk.view),
         "vkCreateImageView failed"))
      return false;
   lt_vk.shown.image_view   = lt_vk.view;
   lt_vk.shown.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   memset(&framebuffer, 0, sizeof(framebuffer));
   framebuffer.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
   framebuffer.renderPass      = lt_vk.pass;
   framebuffer.attachmentCount = 1;
   framebuffer.pAttachments    = &lt_vk.view;
   framebuffer.width           = width;
   framebuffer.height          = height;
   framebuffer.layers          = 1;
   if (!lt_vk_ok(lt_vk.f.create_framebuffer(device, &framebuffer, NULL, &lt_vk.framebuffer),
         "vkCreateFramebuffer failed"))
      return false;
   lt_vk.layout       = VK_IMAGE_LAYOUT_UNDEFINED;
   lt_vk.image_width  = width;
   lt_vk.image_height = height;
   lt_vk.ready        = true;
   return true;
}

/* The render pass, the command pool and its two buffers, the semaphores
 * the handoff uses, and the image */
static void lt_vk_objects(unsigned width, unsigned height)
{
   VkDevice device = lt_vk.vk->device;
   VkAttachmentDescription attachment;
   VkAttachmentReference color;
   VkSubpassDescription subpass;
   VkRenderPassCreateInfo pass;
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo buffers;
   VkSemaphoreCreateInfo semaphore;
   /* Loads: a part draws over what is there */
   memset(&attachment, 0, sizeof(attachment));
   attachment.format         = LT_VK_FORMAT;
   attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
   attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
   attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
   attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   color.attachment          = 0;
   color.layout              = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   memset(&subpass, 0, sizeof(subpass));
   subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments    = &color;
   memset(&pass, 0, sizeof(pass));
   pass.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   pass.attachmentCount = 1;
   pass.pAttachments    = &attachment;
   pass.subpassCount    = 1;
   pass.pSubpasses      = &subpass;
   if (!lt_vk_ok(lt_vk.f.create_render_pass(device, &pass, NULL, &lt_vk.pass),
         "vkCreateRenderPass failed"))
      return;
   memset(&pool, 0, sizeof(pool));
   pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.queueFamilyIndex = lt_vk.vk->queue_index;
   if (!lt_vk_ok(lt_vk.f.create_command_pool(device, &pool, NULL, &lt_vk.pool),
         "vkCreateCommandPool failed"))
      return;
   memset(&buffers, 0, sizeof(buffers));
   buffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   buffers.commandPool        = lt_vk.pool;
   buffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   buffers.commandBufferCount = 2;
   if (!lt_vk_ok(lt_vk.f.allocate_command_buffers(device, &buffers, lt_vk.cmd),
         "vkAllocateCommandBuffers failed"))
      return;
   if (lt_vk.handoff == LT_VK_SEMAPHORE)
   {
      memset(&semaphore, 0, sizeof(semaphore));
      semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      if (   !lt_vk_ok(lt_vk.f.create_semaphore(device, &semaphore, NULL, &lt_vk.drawn),
               "vkCreateSemaphore failed")
          || !lt_vk_ok(lt_vk.f.create_semaphore(device, &semaphore, NULL, &lt_vk.reusable),
               "vkCreateSemaphore failed"))
         return;
   }
   lt_vk_image(width, height);
}

static void RETRO_CALLCONV lt_vk_context_reset(void)
{
   const struct retro_hw_render_interface *iface = NULL;
   struct retro_system_av_info info;
   uint32_t mask;
   lt_vk.said_no_device = false;
   if (!lt_vk.host->environment(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &iface) || !iface)
   {
      lt_vk.host->fail("no Vulkan render interface", 0, 0);
      return;
   }
   if (   iface->interface_type    != RETRO_HW_RENDER_INTERFACE_VULKAN
       || iface->interface_version != RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
   {
      lt_vk.host->fail("a render interface not of Vulkan version 5",
            iface->interface_type, iface->interface_version);
      return;
   }
   lt_vk.vk = (const struct retro_hw_render_interface_vulkan*)iface;
   if (!lt_vk_find_entries())
      return;
   /* One image and one pair of command buffers serve one sync index */
   if ((mask = lt_vk.vk->get_sync_index_mask(lt_vk.vk->handle)) != 1)
   {
      lt_vk.host->fail("a sync index mask that is not 1", mask, 0);
      return;
   }
   retro_get_system_av_info(&info);
   lt_vk_objects(info.geometry.max_width, info.geometry.max_height);
}

static void RETRO_CALLCONV lt_vk_context_destroy(void)
{
   VkDevice device;
   if (lt_vk.entries)
   {
      device = lt_vk.vk->device;
      /* The frontend is done with all the core handed it */
      lt_vk.vk->wait_sync_index(lt_vk.vk->handle);
      lt_vk_free_image();
      lt_vk.f.destroy_semaphore(device, lt_vk.drawn, NULL);
      lt_vk.f.destroy_semaphore(device, lt_vk.reusable, NULL);
      lt_vk.f.destroy_command_pool(device, lt_vk.pool, NULL);
      lt_vk.f.destroy_render_pass(device, lt_vk.pass, NULL);
   }
   lt_vk.drawn    = VK_NULL_HANDLE;
   lt_vk.reusable = VK_NULL_HANDLE;
   lt_vk.pool     = VK_NULL_HANDLE;
   lt_vk.pass     = VK_NULL_HANDLE;
   lt_vk.vk       = NULL;
   lt_vk.entries  = false;
   lt_vk.ready    = false;
   lt_vk.waits    = false;
}

static bool lt_vk_load(const struct lt_render_host *host)
{
   static const char *const negotiates[] = { LT_VK_NEGOTIATE_DEFAULT, "v1", "v2" };
   static const char *const handoffs[]   = { LT_VK_HANDOFF_DEFAULT, "semaphore", "commands" };
   struct retro_hw_render_context_negotiation_interface support;
   unsigned version;
   memset(&lt_vk, 0, sizeof(lt_vk));
   lt_vk.host    = host;
   version       = lt_render_option("link_test_vk_negotiate", negotiates,
         sizeof(negotiates) / sizeof(*negotiates));
   lt_vk.handoff = (enum lt_vk_handoff)lt_render_option("link_test_vk_handoff", handoffs,
         sizeof(handoffs) / sizeof(*handoffs));
   lt_vk.hw.context_type    = RETRO_HW_CONTEXT_VULKAN;
   lt_vk.hw.version_major   = VK_API_VERSION_1_1;
   lt_vk.hw.context_reset   = lt_vk_context_reset;
   lt_vk.hw.context_destroy = lt_vk_context_destroy;
   if (!host->environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &lt_vk.hw))
   {
      host->fail("SET_HW_RENDER refused", lt_vk.hw.context_type, lt_vk.hw.version_major);
      return false;
   }
   if (!version)
      return true;
   memset(&support, 0, sizeof(support));
   support.interface_type = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
   if (!host->environment(RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT,
         &support))
      support.interface_version = 0;
   if (version == 2 && support.interface_version < 2)
   {
      host->fail("no negotiation interface v2", support.interface_version, 0);
      return false;
   }
   lt_vk.negotiation.interface_type       = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
   lt_vk.negotiation.interface_version    = version;
   lt_vk.negotiation.get_application_info = lt_vk_get_application_info;
   lt_vk.negotiation.destroy_device       = lt_vk_destroy_device;
   if (version == 1)
      lt_vk.negotiation.create_device = lt_vk_create_device;
   else
   {
      lt_vk.negotiation.create_instance = lt_vk_create_instance;
      lt_vk.negotiation.create_device2  = lt_vk_create_device2;
   }
   if (host->environment(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
         &lt_vk.negotiation))
      return true;
   host->fail("SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE refused", version, 0);
   return false;
}

/* The image from the layout it is in to layout, after the stages and
 * accesses src name and before those dst name */
static void lt_vk_barrier(VkCommandBuffer cmd, VkImageLayout layout,
      VkPipelineStageFlags src_stages, VkAccessFlags src_access,
      VkPipelineStageFlags dst_stages, VkAccessFlags dst_access)
{
   VkImageMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.srcAccessMask               = src_access;
   barrier.dstAccessMask               = dst_access;
   barrier.oldLayout                   = lt_vk.layout;
   barrier.newLayout                   = layout;
   barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                       = lt_vk.image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = 1;
   barrier.subresourceRange.layerCount = 1;
   lt_vk.f.cmd_pipeline_barrier(cmd, src_stages, dst_stages, 0, 0, NULL, 0, NULL, 1, &barrier);
   lt_vk.layout = layout;
}

/* A part's command buffer begun, in the render pass */
static bool lt_vk_open(VkCommandBuffer cmd)
{
   VkCommandBufferBeginInfo begin;
   VkRenderPassBeginInfo pass;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (!lt_vk_ok(lt_vk.f.begin_command_buffer(cmd, &begin), "vkBeginCommandBuffer failed"))
      return false;
   /* After part 0's writes, or the frontend's reads, which end before
    * this stage starts (libretro_vulkan.h) */
   lt_vk_barrier(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         lt_vk.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
   memset(&pass, 0, sizeof(pass));
   pass.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   pass.renderPass               = lt_vk.pass;
   pass.framebuffer              = lt_vk.framebuffer;
   pass.renderArea.extent.width  = lt_vk.width;
   pass.renderArea.extent.height = lt_vk.height;
   lt_vk.f.cmd_begin_render_pass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
   return true;
}

/* Rows count from the picture's top */
static void lt_vk_fill(VkCommandBuffer cmd, unsigned x, unsigned y,
      unsigned width, unsigned height, uint32_t rgb)
{
   VkClearAttachment clear;
   VkClearRect rect;
   memset(&clear, 0, sizeof(clear));
   clear.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
   clear.clearValue.color.float32[0] = (float)((rgb >> 16) & 0xFF) / 255.0f;
   clear.clearValue.color.float32[1] = (float)((rgb >> 8) & 0xFF) / 255.0f;
   clear.clearValue.color.float32[2] = (float)(rgb & 0xFF) / 255.0f;
   clear.clearValue.color.float32[3] = 1.0f;
   rect.rect.offset.x      = (int32_t)x;
   rect.rect.offset.y      = (int32_t)y;
   rect.rect.extent.width  = width;
   rect.rect.extent.height = height;
   rect.baseArrayLayer     = 0;
   rect.layerCount         = 1;
   lt_vk.f.cmd_clear_attachments(cmd, 1, &clear, 1, &rect);
}

/* A part's command buffer ended and handed over: submitted, or part 1's
 * given to the frontend to submit */
static void lt_vk_hand_over(unsigned part)
{
   static const VkPipelineStageFlags waits_at = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   const struct retro_hw_render_interface_vulkan *vk = lt_vk.vk;
   VkSubmitInfo submit;
   VkResult result;
   if (!lt_vk_ok(lt_vk.f.end_command_buffer(lt_vk.cmd[part]), "vkEndCommandBuffer failed"))
   {
      lt_vk.drawing = false;
      return;
   }
   if (part == 1 && lt_vk.handoff == LT_VK_COMMANDS)
   {
      vk->set_command_buffers(vk->handle, 1, &lt_vk.cmd[1]);
      return;
   }
   memset(&submit, 0, sizeof(submit));
   submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers    = &lt_vk.cmd[part];
   if (part == 0 && lt_vk.waits)
   {
      submit.waitSemaphoreCount = 1;
      submit.pWaitSemaphores    = &lt_vk.reusable;
      submit.pWaitDstStageMask  = &waits_at;
      lt_vk.waits               = false;
   }
   if (part == 1 && lt_vk.handoff == LT_VK_SEMAPHORE)
   {
      submit.signalSemaphoreCount = 1;
      submit.pSignalSemaphores    = &lt_vk.drawn;
   }
   vk->lock_queue(vk->handle);
   result = lt_vk.f.queue_submit(vk->queue, 1, &submit, VK_NULL_HANDLE);
   vk->unlock_queue(vk->handle);
   if (!lt_vk_ok(result, "vkQueueSubmit failed"))
      lt_vk.drawing = false;
}

static void lt_vk_begin(unsigned width, unsigned height)
{
   lt_vk.width   = width;
   lt_vk.height  = height;
   lt_vk.drawing = false;
   if (!lt_render_tell(width, height))
      lt_vk.host->fail("SET_SYSTEM_AV_INFO refused", width, height);
   if (!lt_vk.ready)
   {
      if (!lt_vk.said_no_device)
         lt_vk.host->fail("a frame with no Vulkan device", 0, 0);
      lt_vk.said_no_device = true;
      return;
   }
   /* The image and both command buffers are the core's again */
   lt_vk.vk->wait_sync_index(lt_vk.vk->handle);
   if (     (width > lt_vk.image_width || height > lt_vk.image_height)
         && !lt_vk_image(width > lt_vk.image_width ? width : lt_vk.image_width,
               height > lt_vk.image_height ? height : lt_vk.image_height))
   {
      lt_vk.said_no_device = true;
      return;
   }
   if (   !lt_vk_ok(lt_vk.f.reset_command_pool(lt_vk.vk->device, lt_vk.pool, 0),
            "vkResetCommandPool failed")
       || !lt_vk_open(lt_vk.cmd[0]))
      return;
   lt_vk_fill(lt_vk.cmd[0], 0, 0, width, height, 0);
   lt_vk.drawing = true;
}

/* Part 0 goes on in the command buffer begin opened */
static void lt_vk_part(unsigned part, uint64_t value)
{
   VkCommandBuffer cmd = lt_vk.cmd[part];
   unsigned i;
   if (!lt_vk.drawing)
      return;
   if (part == 1 && !lt_vk_open(cmd))
   {
      lt_vk.drawing = false;
      return;
   }
   for (i = 0; i < LT_RENDER_CELLS; i++)
   {
      struct lt_render_cell c;
      lt_render_cell(lt_vk.width, lt_vk.height, part, value, i, &c);
      lt_vk_fill(cmd, c.x, c.y, c.width, c.height, c.rgb);
   }
   lt_vk.f.cmd_end_render_pass(cmd);
   /* The frontend samples the picture, libretro_vulkan.h's example says */
   if (part == 1)
      lt_vk_barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
   lt_vk_hand_over(part);
}

static void lt_vk_present(retro_video_refresh_t video, bool dupe)
{
   const struct retro_hw_render_interface_vulkan *vk = lt_vk.vk;
   bool semaphores = lt_vk.handoff == LT_VK_SEMAPHORE;
   if (lt_vk.drawing && !dupe)
      vk->set_image(vk->handle, &lt_vk.shown, semaphores ? 1 : 0,
            semaphores ? &lt_vk.drawn : NULL, VK_QUEUE_FAMILY_IGNORED);
   if (lt_vk.drawing && semaphores)
   {
      /* Signalled in this video call, a dupe's too */
      vk->set_signal_semaphore(vk->handle, lt_vk.reusable);
      lt_vk.waits = true;
   }
   video(dupe || !lt_vk.drawing ? NULL : RETRO_HW_FRAME_BUFFER_VALID,
         lt_vk.width, lt_vk.height, 0);
}

/* context_destroy has freed every object */
static void lt_vk_unload(void)
{
}

const struct lt_render lt_render_vk = {
   lt_vk_load, lt_vk_begin, lt_vk_part, lt_vk_present, lt_vk_unload
};

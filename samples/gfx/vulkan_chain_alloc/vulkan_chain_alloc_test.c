/* gfx/drivers_shader/shader_vulkan.c's filter chain creation with each
 * allocation it makes failed in turn: slang_chain_new() gives a chain
 * with every pass and every deferred-disposal queue it was asked for,
 * or none, and what a failed one made is freed (ASan, LSan).
 *
 * The file is compiled into this one (slang_chain_new() is static)
 * with malloc(), calloc() and realloc() routed through the test. The
 * Vulkan entry points it calls go through the real symbol wrapper,
 * pointed here at mocks that hand out handles and host memory; the
 * rest of what the file links against is stood in for in stubs.c. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned alloc_calls; /* allocations so far */
static unsigned fail_at;     /* the one to fail, 1-based; 0 none */

static void *test_malloc(size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return malloc(size);
}

static void *test_calloc(size_t n, size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return calloc(n, size);
}

static void *test_realloc(void *p, size_t size)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return realloc(p, size);
}

#define malloc(size)     test_malloc(size)
#define calloc(n, size)  test_calloc(n, size)
#define realloc(p, size) test_realloc(p, size)
#include "../../../gfx/drivers_shader/shader_vulkan.c"
#undef malloc
#undef calloc
#undef realloc

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- Vulkan ---- */
static uint64_t next_handle = 1;
static int      live_objects;

#define MOCK_CREATE(name, Info, Handle) \
   static VkResult VKAPI_CALL name(VkDevice d, const Info *i, \
         const VkAllocationCallbacks *a, Handle *out) \
   { (void)d; (void)i; (void)a; *out = (Handle)(uintptr_t)next_handle++; \
     live_objects++; return VK_SUCCESS; }
#define MOCK_DESTROY(name, Handle) \
   static void VKAPI_CALL name(VkDevice d, Handle h, \
         const VkAllocationCallbacks *a) \
   { (void)d; (void)a; if (h != VK_NULL_HANDLE) live_objects--; }

MOCK_CREATE(mock_create_buffer,  VkBufferCreateInfo,  VkBuffer)
MOCK_DESTROY(mock_destroy_buffer, VkBuffer)
MOCK_CREATE(mock_create_sampler, VkSamplerCreateInfo, VkSampler)
MOCK_DESTROY(mock_destroy_sampler, VkSampler)

static VkResult VKAPI_CALL mock_allocate_memory(VkDevice d,
      const VkMemoryAllocateInfo *i, const VkAllocationCallbacks *a,
      VkDeviceMemory *out)
{
   void *host = malloc((size_t)i->allocationSize);
   (void)d; (void)a;
   if (!host)
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   *out = (VkDeviceMemory)(uintptr_t)host;
   live_objects++;
   return VK_SUCCESS;
}

static void VKAPI_CALL mock_free_memory(VkDevice d, VkDeviceMemory m,
      const VkAllocationCallbacks *a)
{
   (void)d; (void)a;
   if (m != VK_NULL_HANDLE)
   {
      free((void*)(uintptr_t)m);
      live_objects--;
   }
}

static VkResult VKAPI_CALL mock_map_memory(VkDevice d, VkDeviceMemory m,
      VkDeviceSize off, VkDeviceSize size, VkMemoryMapFlags f, void **p)
{
   (void)d; (void)size; (void)f;
   *p = (char*)(uintptr_t)m + off;
   return VK_SUCCESS;
}

static void VKAPI_CALL mock_unmap_memory(VkDevice d, VkDeviceMemory m)
{ (void)d; (void)m; }

static void VKAPI_CALL mock_buffer_requirements(VkDevice d, VkBuffer b,
      VkMemoryRequirements *r)
{
   (void)d; (void)b;
   r->size           = 4096;
   r->alignment      = 256;
   r->memoryTypeBits = 1;
}

static VkResult VKAPI_CALL mock_bind_buffer_memory(VkDevice d, VkBuffer b,
      VkDeviceMemory m, VkDeviceSize off)
{ (void)d; (void)b; (void)m; (void)off; return VK_SUCCESS; }

static void mock_install(void)
{
   vkCreateBuffer               = mock_create_buffer;
   vkDestroyBuffer              = mock_destroy_buffer;
   vkCreateSampler              = mock_create_sampler;
   vkDestroySampler             = mock_destroy_sampler;
   vkAllocateMemory             = mock_allocate_memory;
   vkFreeMemory                 = mock_free_memory;
   vkMapMemory                  = mock_map_memory;
   vkUnmapMemory                = mock_unmap_memory;
   vkGetBufferMemoryRequirements= mock_buffer_requirements;
   vkBindBufferMemory           = mock_bind_buffer_memory;
}

/* gfx/common/vulkan_common.c's memory helpers, over the mocks: the
 * device has one memory type */
uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{ (void)mem_props; (void)device_reqs; (void)host_reqs; return 0; }

VkResult vulkan_allocate_cpu_write_memory(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t type_bits, const VkMemoryAllocateInfo *alloc,
      VkDeviceMemory *mem)
{
   (void)mem_props; (void)type_bits;
   return vkAllocateMemory(device, alloc, NULL, mem);
}

static void wait_submissions(void *handle) { (void)handle; }

#define PASSES  3
#define INDICES 3

static bool chain_whole(const struct vulkan_filter_chain *chain)
{
   return chain->pass_count == PASSES && chain->num_deferred == INDICES;
}

int main(void)
{
   VkPhysicalDeviceMemoryProperties props;
   struct vulkan_filter_chain_create_info info;
   struct vulkan_filter_chain *chain;
   unsigned allocs, k;

   mock_install();

   memset(&props, 0, sizeof(props));
   props.memoryTypeCount             = 1;
   props.memoryTypes[0].propertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

   memset(&info, 0, sizeof(info));
   info.device                = (VkDevice)(uintptr_t)0x1000;
   info.memory_properties     = &props;
   info.wait_submissions      = wait_submissions;
   info.num_passes            = PASSES;
   info.original_format       = VK_FORMAT_R8G8B8A8_UNORM;
   info.max_input_dims        = VIDEO_SCALE_PACK(256, 256);
   info.swapchain.format      = VK_FORMAT_B8G8R8A8_UNORM;
   info.swapchain.num_indices = INDICES;

   alloc_calls = 0;
   fail_at     = 0;
   chain       = slang_chain_new(&info);
   allocs      = alloc_calls;
   CHECK(chain && chain_whole(chain), "with memory: %u passes, %u queues",
         PASSES, INDICES);
   slang_chain_free(chain);
   CHECK(live_objects == 0, "with memory: %d Vulkan objects left",
         live_objects);

   for (k = 1; k <= allocs; k++)
   {
      alloc_calls = 0;
      fail_at     = k;
      chain       = slang_chain_new(&info);
      fail_at     = 0;
      CHECK(!chain || chain_whole(chain),
            "allocation %u of %u failed and slang_chain_new gave a chain "
            "of %u passes and %u queues", k, allocs,
            (unsigned)chain->pass_count, (unsigned)chain->num_deferred);
      slang_chain_free(chain);
      CHECK(live_objects == 0, "allocation %u of %u failed and %d Vulkan "
            "objects were left", k, allocs, live_objects);
      live_objects = 0;
   }

   if (failures)
   {
      printf("vulkan_chain_alloc_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("vulkan_chain_alloc_test: OK (%u allocations)\n", allocs);
   return 0;
}

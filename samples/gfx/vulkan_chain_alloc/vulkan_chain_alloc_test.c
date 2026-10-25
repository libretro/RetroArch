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

/* The render pass a target is drawn with, as created */
static VkAttachmentLoadOp last_load_op;
static VkResult VKAPI_CALL mock_create_render_pass(VkDevice d,
      const VkRenderPassCreateInfo *info, const VkAllocationCallbacks *a,
      VkRenderPass *out)
{
   (void)d; (void)a;
   last_load_op = info->pAttachments[0].loadOp;
   *out = (VkRenderPass)(uintptr_t)0x2000;
   return VK_SUCCESS;
}

/* What a pass's commands did to its target */
static VkImageLayout first_old_layout;
static unsigned      barriers, clears;
static VkRect2D      render_area;
static void VKAPI_CALL mock_barrier(VkCommandBuffer c, VkPipelineStageFlags s,
      VkPipelineStageFlags d, VkDependencyFlags f, uint32_t mn,
      const VkMemoryBarrier *m, uint32_t bn, const VkBufferMemoryBarrier *b,
      uint32_t in, const VkImageMemoryBarrier *i)
{
   (void)c; (void)s; (void)d; (void)f; (void)mn; (void)m; (void)bn; (void)b;
   if (in && !barriers++)
      first_old_layout = i[0].oldLayout;
}
static void VKAPI_CALL mock_clear_image(VkCommandBuffer c, VkImage i,
      VkImageLayout l, const VkClearColorValue *v, uint32_t n,
      const VkImageSubresourceRange *r)
{ (void)c; (void)i; (void)l; (void)v; (void)n; (void)r; clears++; }
static void VKAPI_CALL mock_begin_render_pass(VkCommandBuffer c,
      const VkRenderPassBeginInfo *info, VkSubpassContents s)
{ (void)c; (void)s; render_area = info->renderArea; }
static void VKAPI_CALL mock_cmd_nop(void) { }

/* A subframe of the rolling scanline simulation draws one band of a
 * pass's target and keeps the rest: the render area is the band and
 * the target is not discarded, cleared once when new; any other frame
 * draws the whole target from undefined contents. */
static void test_scanline_band(struct vulkan_filter_chain *chain)
{
   struct slang_pass *pass = chain->passes[0];
   struct slang_framebuffer fb;
   Texture original, source;
   VkViewport vp;
   VkDescriptorSet sets[INDICES];
   unsigned frame;

   vkCmdPipelineBarrier   = mock_barrier;
   vkCmdClearColorImage   = mock_clear_image;
   vkCmdBeginRenderPass   = mock_begin_render_pass;
   vkCmdBindPipeline      = (PFN_vkCmdBindPipeline)mock_cmd_nop;
   vkCmdBindDescriptorSets= (PFN_vkCmdBindDescriptorSets)mock_cmd_nop;
   vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)mock_cmd_nop;
   vkCmdSetViewport       = (PFN_vkCmdSetViewport)mock_cmd_nop;
   vkCmdSetScissor        = (PFN_vkCmdSetScissor)mock_cmd_nop;
   vkCmdDraw              = (PFN_vkCmdDraw)mock_cmd_nop;
   vkCmdEndRenderPass     = (PFN_vkCmdEndRenderPass)mock_cmd_nop;

   memset(&fb, 0, sizeof(fb));
   fb.size_dims   = VIDEO_SCALE_PACK(64, 64);
   fb.image       = (VkImage)(uintptr_t)0x3000;
   fb.levels      = 1;
   memset(&original, 0, sizeof(original));
   memset(&source, 0, sizeof(source));
   original.texture.dims = source.texture.dims = VIDEO_SCALE_PACK(64, 64);
   memset(&vp, 0, sizeof(vp));
   memset(sets, 0, sizeof(sets));
   pass->framebuffer            = &fb;
   pass->sets                   = sets;
   pass->final_pass             = false;
   pass->pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
   pass->pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
   pass->pass_info.scale_x      = 1.0f;
   pass->pass_info.scale_y      = 1.0f;

   pass->common->simulate_scanline = true;
   pass->common->total_subframes   = 4;
   for (frame = 0; frame < 2; frame++)
   {
      pass->common->current_subframe = 2;
      barriers = clears = 0;
      slang_pass_build_commands(pass, NULL, VK_NULL_HANDLE, &original,
            &source, &vp, NULL, true);
      CHECK(render_area.offset.y == 16 && render_area.extent.height == 16
            && render_area.extent.width == 64,
            "subframe 2 of 4 draws rows 16-31: %d + %u", render_area.offset.y,
            render_area.extent.height);
      CHECK(clears == (frame ? 0u : 1u), "the new target is cleared once: %u", clears);
      CHECK(first_old_layout == (frame ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_UNDEFINED) && fb.defined,
            "the target keeps its contents");
   }

   pass->common->simulate_scanline = false;
   barriers = clears = 0;
   slang_pass_build_commands(pass, NULL, VK_NULL_HANDLE, &original,
         &source, &vp, NULL, true);
   CHECK(render_area.offset.y == 0 && render_area.extent.height == 64 && !clears
         && first_old_layout == VK_IMAGE_LAYOUT_UNDEFINED,
         "without the simulation the whole target is drawn, contents discarded");
   pass->framebuffer = NULL;
   pass->sets        = NULL;
}

/* A pass whose fragment stage can discard draws to a target cleared to
 * transparent black, as glcore and D3D clear theirs; one that writes
 * every pixel keeps the cheaper don't-care load. */
static void test_discard_clears(void)
{
   /* header; OpConstant %t %c 252 (an operand that looks like OpKill);
    * then OpKill, OpTerminateInvocation or OpDemoteToHelperInvocation */
   uint32_t words[10] = { 0x07230203, 0x10000, 0, 16, 0,
      (4u << 16) | 43, 1, 2, 252, 0 };
   VkRenderPass rp = VK_NULL_HANDLE;

   words[9] = (1u << 16) | 253;   /* OpReturn */
   CHECK(!spirv_can_discard(words, 10), "a stage without discard");
   words[9] = (1u << 16) | 252;
   CHECK(spirv_can_discard(words, 10), "OpKill");
   words[9] = (1u << 16) | 4416;
   CHECK(spirv_can_discard(words, 10), "OpTerminateInvocation");
   words[9] = (1u << 16) | 5380;
   CHECK(spirv_can_discard(words, 10), "OpDemoteToHelperInvocation");
   CHECK(!spirv_can_discard(NULL, 0), "no stage");

   vkCreateRenderPass = mock_create_render_pass;
   CHECK(vulkan_initialize_render_pass((VkDevice)(uintptr_t)0x1000,
            VK_FORMAT_R8G8B8A8_UNORM, true, &rp)
         && last_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR,
         "a discarding pass's target is cleared");
   CHECK(vulkan_initialize_render_pass((VkDevice)(uintptr_t)0x1000,
            VK_FORMAT_R8G8B8A8_UNORM, false, &rp)
         && last_load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE,
         "any other pass's target is not");
}

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

   test_discard_clears();
   chain = slang_chain_new(&info);
   CHECK(chain != NULL, "a chain");
   if (chain)
   {
      test_scanline_band(chain);
      slang_chain_free(chain);
   }

   if (failures)
   {
      printf("vulkan_chain_alloc_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("vulkan_chain_alloc_test: OK (%u allocations)\n", allocs);
   return 0;
}

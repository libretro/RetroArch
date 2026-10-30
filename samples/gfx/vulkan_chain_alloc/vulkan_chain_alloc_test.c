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

/* ---- Replay ---- */
MOCK_CREATE(mock_create_image, VkImageCreateInfo, VkImage)
MOCK_DESTROY(mock_destroy_image, VkImage)
MOCK_CREATE(mock_create_image_view, VkImageViewCreateInfo, VkImageView)
MOCK_DESTROY(mock_destroy_image_view, VkImageView)
MOCK_CREATE(mock_create_framebuffer, VkFramebufferCreateInfo, VkFramebuffer)
MOCK_DESTROY(mock_destroy_framebuffer, VkFramebuffer)
MOCK_DESTROY(mock_destroy_render_pass, VkRenderPass)

static void VKAPI_CALL mock_image_requirements(VkDevice d, VkImage i,
      VkMemoryRequirements *r)
{
   (void)d; (void)i;
   r->size           = 4096;
   r->alignment      = 256;
   r->memoryTypeBits = 1;
}

static VkResult VKAPI_CALL mock_bind_image_memory(VkDevice d, VkImage i,
      VkDeviceMemory m, VkDeviceSize off)
{ (void)d; (void)i; (void)m; (void)off; return VK_SUCCESS; }

/* The view each binding of the final pass's set was last written. */
static VkImageView written_view[4];
static void VKAPI_CALL mock_update_sets(VkDevice d, uint32_t n,
      const VkWriteDescriptorSet *w, uint32_t cn, const VkCopyDescriptorSet *c)
{
   uint32_t i;
   (void)d; (void)cn; (void)c;
   for (i = 0; i < n; i++)
      if (w[i].dstBinding < 4 && w[i].pImageInfo)
         written_view[w[i].dstBinding] = w[i].pImageInfo[0].imageView;
}

/* A replay of the last frame's final pass, in a later frame and at
 * another size, reads what that pass read: the second pass's output as
 * the frame left it - not the framebuffer the feedback swap gave it -
 * that pass's feedback, the history the frame saw, and that frame's
 * count; and swaps no feedback. */
static void test_replay(struct vulkan_filter_chain *chain)
{
   struct slang_pass *last = chain->passes[PASSES - 1];
   struct slang_pass *prev = chain->passes[PASSES - 2];
   slang_texture_semantic_meta source_meta[1], feedback_meta[PASSES - 1];
   slang_texture_semantic_meta history_meta[2];
   slang_texture_semantic_meta unread[SLANG_NUM_TEXTURE_SEMANTICS];
   struct slang_framebuffer swapped, feedback;
   unsigned i;
   VkDescriptorSet sets[INDICES];
   uint32_t push[4];
   VkViewport vp;
   float mvp[16];

   vkUpdateDescriptorSets = mock_update_sets;
   vkCmdPipelineBarrier   = mock_barrier;
   vkCmdBindPipeline      = (PFN_vkCmdBindPipeline)mock_cmd_nop;
   vkCmdBindDescriptorSets= (PFN_vkCmdBindDescriptorSets)mock_cmd_nop;
   vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)mock_cmd_nop;
   vkCmdSetViewport       = (PFN_vkCmdSetViewport)mock_cmd_nop;
   vkCmdSetScissor        = (PFN_vkCmdSetScissor)mock_cmd_nop;
   vkCmdDraw              = (PFN_vkCmdDraw)mock_cmd_nop;

   CHECK(!vulkan_filter_chain_replay_ready(chain),
         "a chain with no frame drawn is ready to replay");

   memset(unread, 0, sizeof(unread));
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      last->reflection.semantic_textures[i].data = &unread[i];
      last->reflection.semantic_textures[i].size = 1;
   }
   memset(source_meta, 0, sizeof(source_meta));
   memset(feedback_meta, 0, sizeof(feedback_meta));
   memset(history_meta, 0, sizeof(history_meta));
   source_meta[0].texture              = true;
   source_meta[0].binding              = 0;
   feedback_meta[PASSES - 2].texture   = true;
   feedback_meta[PASSES - 2].binding   = 1;
   history_meta[1].texture             = true;
   history_meta[1].binding             = 2;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].data = source_meta;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_SOURCE].size = 1;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].data = feedback_meta;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK].size = PASSES - 1;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].data = history_meta;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size = 2;
   last->reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT].push_constant = true;
   last->reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT].push_constant_offset = 0;
   memset(push, 0, sizeof(push));
   memset(sets, 0, sizeof(sets));
   last->push.buffer = push;
   last->push.stages = 0;
   last->sets        = sets;
   last->final_pass  = true;
   last->framebuffer = NULL;

   /* As the last frame left the chain: the second pass's output was
    * frame A, the feedback swap gave it the other framebuffer. */
   memset(&swapped, 0, sizeof(swapped));
   memset(&feedback, 0, sizeof(feedback));
   swapped.view       = (VkImageView)(uintptr_t)0xB1;
   swapped.size_dims  = VIDEO_SCALE_PACK(64, 32);
   feedback.view      = (VkImageView)(uintptr_t)0xA1;
   feedback.size_dims = VIDEO_SCALE_PACK(64, 32);
   prev->framebuffer  = &swapped;
   prev->fb_feedback  = &feedback;
   texture_array_resize(&chain->common.pass_outputs,
         &chain->common.num_pass_outputs, PASSES);
   texture_array_resize(&chain->common.fb_feedback,
         &chain->common.num_fb_feedback, PASSES - 1);
   texture_array_resize(&chain->common.original_history,
         &chain->common.num_original_history, 1);
   chain->common.pass_outputs[PASSES - 2].texture.view = (VkImageView)(uintptr_t)0xA1;
   chain->common.pass_outputs[PASSES - 2].texture.dims = VIDEO_SCALE_PACK(64, 32);
   chain->common.fb_feedback[PASSES - 2].texture.view  = (VkImageView)(uintptr_t)0xF1;
   chain->common.fb_feedback[PASSES - 2].texture.dims  = VIDEO_SCALE_PACK(64, 32);
   chain->common.original_history[0].texture.view      = (VkImageView)(uintptr_t)0xC1;
   chain->common.original_history[0].texture.dims      = VIDEO_SCALE_PACK(64, 32);
   chain->input_texture.view  = (VkImageView)(uintptr_t)0xD1;
   chain->input_texture.dims  = VIDEO_SCALE_PACK(64, 32);
   chain->drawn             = true;
   chain->drawn_frame_count = 7;
   last->frame_count        = 9;

   CHECK(vulkan_filter_chain_replay_ready(chain), "no replay after a frame");
   memset(&vp, 0, sizeof(vp));
   vp.width  = 80.0f;
   vp.height = 40.0f;
   build_identity_matrix(mvp);
   memset(written_view, 0, sizeof(written_view));
   vulkan_filter_chain_build_viewport_pass_replay(chain, VK_NULL_HANDLE,
         &vp, mvp);
   CHECK(written_view[0] == (VkImageView)(uintptr_t)0xA1,
         "replay Source: view %p, want the frame's output",
         (void*)(uintptr_t)written_view[0]);
   CHECK(written_view[1] == (VkImageView)(uintptr_t)0xF1,
         "replay PassFeedback: view %p", (void*)(uintptr_t)written_view[1]);
   CHECK(written_view[2] == (VkImageView)(uintptr_t)0xC1,
         "replay OriginalHistory1: view %p", (void*)(uintptr_t)written_view[2]);
   CHECK(push[0] == 7, "replay drew with count %u, want 7", push[0]);
   CHECK(last->frame_count == 9, "replay left the count at %u",
         (unsigned)last->frame_count);
   CHECK(prev->framebuffer == &swapped && prev->fb_feedback == &feedback,
         "replay swapped the feedback buffers");

   prev->framebuffer = NULL;
   prev->fb_feedback = NULL;
   memset(&last->reflection, 0, sizeof(last->reflection));
   last->push.buffer = NULL;
   last->sets        = NULL;
}

/* A chain made to replay keeps one history framebuffer more than its
 * passes read, so the frame's input goes where the last frame did not
 * read: a replay still finds that frame's history. */
static void test_history_spare(struct vulkan_filter_chain *chain)
{
   slang_texture_semantic_meta history_meta[3];
   struct slang_pass *last = chain->passes[PASSES - 1];
   unsigned before;

   vkCreateImage                = mock_create_image;
   vkDestroyImage               = mock_destroy_image;
   vkCreateImageView            = mock_create_image_view;
   vkDestroyImageView           = mock_destroy_image_view;
   vkCreateFramebuffer          = mock_create_framebuffer;
   vkDestroyFramebuffer         = mock_destroy_framebuffer;
   vkCreateRenderPass           = mock_create_render_pass;
   vkDestroyRenderPass          = mock_destroy_render_pass;
   vkGetImageMemoryRequirements = mock_image_requirements;
   vkBindImageMemory            = mock_bind_image_memory;

   memset(history_meta, 0, sizeof(history_meta));
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].data = history_meta;
   last->reflection.semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size = 3;
   /* Any other chain keeps only what its passes read. */
   chain->replay = false;
   CHECK(slang_chain_init_history(chain), "history not made");
   CHECK(chain->num_history == 2 && chain->common.num_original_history == 2,
         "without replay: %u framebuffers for %u read",
         (unsigned)chain->num_history,
         (unsigned)chain->common.num_original_history);
   chain->replay = true;
   CHECK(slang_chain_init_history(chain), "history not made");
   CHECK(chain->num_history == 3 && chain->common.num_original_history == 2,
         "history: %u framebuffers for %u read",
         (unsigned)chain->num_history,
         (unsigned)chain->common.num_original_history);
   if (chain->num_history == 3)
   {
      chain->history_ring_index = 0;
      slang_chain_update_history_info(chain);
      before = chain->history_ring_index;
      /* What the frame's end writes: the slot before the newest. */
      CHECK((before + chain->num_history - 1) % chain->num_history
            != (before + 0) % chain->num_history
            && (before + chain->num_history - 1) % chain->num_history
            != (before + 1) % chain->num_history,
            "the frame's end writes a slot the frame read");
      CHECK(chain->common.original_history[1].texture.image
            == chain->original_history[1]->image,
            "history 2 is not the ring's second slot");
   }
   memset(&last->reflection, 0, sizeof(last->reflection));
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
      test_replay(chain);
      test_history_spare(chain);
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

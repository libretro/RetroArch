/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2016 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "shader_vulkan.h"
#include "glslang_util.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include <string.h>
#include <math.h>
#include "../drivers/vulkan_shaders/opaque.vert.inc"
#include "../drivers/vulkan_shaders/opaque.frag.inc"
#include "../video_shader_driver.h"
#include "../../verbosity.h"
#include "slang_reflection.hpp"

using namespace std;

static void image_layout_transition(
      VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags src_access, VkAccessFlags dst_access,
      VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages)
{
   VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = src_access;
   barrier.dstAccessMask               = dst_access;
   barrier.oldLayout                   = old_layout;
   barrier.newLayout                   = new_layout;
   barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = 1;
   barrier.subresourceRange.layerCount = 1;

   VKFUNC(vkCmdPipelineBarrier)(cmd,
         src_stages,
         dst_stages,
         false,
         0, nullptr,
         0, nullptr,
         1, &barrier);
}

static uint32_t find_memory_type(
      const VkPhysicalDeviceMemoryProperties &mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
   abort();
}

static uint32_t find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties &mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   return find_memory_type(mem_props, device_reqs, 0);
}

static void build_identity_matrix(float *data)
{
   data[ 0] = 1.0f;
   data[ 1] = 0.0f;
   data[ 2] = 0.0f;
   data[ 3] = 0.0f;
   data[ 4] = 0.0f;
   data[ 5] = 1.0f;
   data[ 6] = 0.0f;
   data[ 7] = 0.0f;
   data[ 8] = 0.0f;
   data[ 9] = 0.0f;
   data[10] = 1.0f;
   data[11] = 0.0f;
   data[12] = 0.0f;
   data[13] = 0.0f;
   data[14] = 0.0f;
   data[15] = 1.0f;
}

static void build_vec4(float *data, unsigned width, unsigned height)
{
   data[0] = float(width);
   data[1] = float(height);
   data[2] = 1.0f / float(width);
   data[3] = 1.0f / float(height);
}

struct Size2D
{
   unsigned width, height;
};

struct Texture
{
   vulkan_filter_chain_texture texture;
   vulkan_filter_chain_filter filter;
};

class DeferredDisposer
{
   public:
      DeferredDisposer(vector<function<void ()>> &calls) : calls(calls) {}

      void defer(function<void ()> func)
      {
         calls.push_back(move(func));
      }

   private:
      vector<function<void ()>> &calls;
};

class Buffer
{
   public:
      Buffer(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &mem_props,
            size_t size, VkBufferUsageFlags usage);
      ~Buffer();

      size_t get_size() const { return size; }
      void *map();
      void unmap();

      const VkBuffer &get_buffer() const { return buffer; }

   private:
      VkDevice device;
      VkBuffer buffer;
      VkDeviceMemory memory;
      size_t size;
};

class Framebuffer
{
   public:
      Framebuffer(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &mem_props,
            const Size2D &max_size, VkFormat format);

      ~Framebuffer();
      Framebuffer(Framebuffer&&) = delete;
      void operator=(Framebuffer&&) = delete;

      void set_size(DeferredDisposer &disposer, const Size2D &size);

      const Size2D &get_size() const { return size; }
      VkImage get_image() const { return image; }
      VkImageView get_view() const { return view; }
      VkFramebuffer get_framebuffer() const { return framebuffer; }
      VkRenderPass get_render_pass() const { return render_pass; }

      void clear(VkCommandBuffer cmd);
      void copy(VkCommandBuffer cmd, VkImage image, VkImageLayout layout);

   private:
      VkDevice device = VK_NULL_HANDLE;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkImage image = VK_NULL_HANDLE;
      VkImageView view = VK_NULL_HANDLE;
      Size2D size;
      VkFormat format;

      VkFramebuffer framebuffer = VK_NULL_HANDLE;
      VkRenderPass render_pass = VK_NULL_HANDLE;

      struct
      {
         size_t size = 0;
         uint32_t type = 0;
         VkDeviceMemory memory = VK_NULL_HANDLE;
      } memory;

      void init(DeferredDisposer *disposer);
      void init_framebuffer();
      void init_render_pass();
};

struct CommonResources
{
   CommonResources(VkDevice device,
         const VkPhysicalDeviceMemoryProperties &memory_properties);
   ~CommonResources();

   unique_ptr<Buffer> vbo_offscreen, vbo_final;
   VkSampler samplers[2];

   vector<Texture> original_history;
   vector<Texture> framebuffer_feedback;

   VkDevice device;
};

class Pass
{
   public:
      Pass(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &memory_properties,
            VkPipelineCache cache, unsigned num_sync_indices, bool final_pass) :
         device(device),
         memory_properties(memory_properties),
         cache(cache),
         num_sync_indices(num_sync_indices),
         final_pass(final_pass)
      {}

      ~Pass();

      Pass(Pass&&) = delete;
      void operator=(Pass&&) = delete;

      const Framebuffer &get_framebuffer() const
      {
         return *framebuffer;
      }

      Framebuffer *get_feedback_framebuffer()
      {
         return framebuffer_feedback.get();
      }

      Size2D set_pass_info(
            const Size2D &max_original,
            const Size2D &max_source,
            const vulkan_filter_chain_swapchain_info &swapchain,
            const vulkan_filter_chain_pass_info &info);

      void set_shader(VkShaderStageFlags stage,
            const uint32_t *spirv,
            size_t spirv_words);

      bool build();
      bool init_feedback();

      void build_commands(
            DeferredDisposer &disposer,
            VkCommandBuffer cmd,
            const Texture &original,
            const Texture &source,
            const VkViewport &vp,
            const float *mvp);

      void notify_sync_index(unsigned index)
      {
         sync_index = index;
      }

      vulkan_filter_chain_filter get_source_filter() const
      {
         return pass_info.source_filter;
      }

      void set_common_resources(const CommonResources &common)
      {
         this->common = &common;
      }

      const slang_reflection &get_reflection() const
      {
         return reflection;
      }

      void end_frame();

   private:
      VkDevice device;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkPipelineCache cache;
      unsigned num_sync_indices;
      unsigned sync_index;
      bool final_pass;

      Size2D get_output_size(const Size2D &original_size,
            const Size2D &max_source) const;

      VkPipeline pipeline = VK_NULL_HANDLE;
      VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
      VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
      VkDescriptorPool pool = VK_NULL_HANDLE;

      vector<unique_ptr<Buffer>> ubos;
      vector<VkDescriptorSet> sets;
      const CommonResources *common = nullptr;

      Size2D current_framebuffer_size;
      VkViewport current_viewport;
      vulkan_filter_chain_pass_info pass_info;

      vector<uint32_t> vertex_shader;
      vector<uint32_t> fragment_shader;
      unique_ptr<Framebuffer> framebuffer;
      unique_ptr<Framebuffer> framebuffer_feedback;
      VkRenderPass swapchain_render_pass;

      void clear_vk();
      bool init_pipeline();
      bool init_pipeline_layout();
      bool init_buffers();

      void set_texture(VkDescriptorSet set, unsigned binding,
            const Texture &texture);

      void set_semantic_texture(VkDescriptorSet set, slang_texture_semantic semantic,
            const Texture &texture);
      void set_semantic_texture_array(VkDescriptorSet set,
            slang_texture_semantic semantic, unsigned index,
            const Texture &texture);

      void set_uniform_buffer(VkDescriptorSet set, unsigned binding,
            VkBuffer buffer,
            VkDeviceSize offset,
            VkDeviceSize range);

      slang_reflection reflection;
      void build_semantics(VkDescriptorSet set, uint8_t *buffer,
            const float *mvp, const Texture &original, const Texture &source);
      void build_semantic_vec4(uint8_t *data, slang_semantic semantic,
            unsigned width, unsigned height);
      void build_semantic_texture_vec4(uint8_t *data,
            slang_texture_semantic semantic,
            unsigned width, unsigned height);
      void build_semantic_texture_array_vec4(uint8_t *data,
            slang_texture_semantic semantic, unsigned index,
            unsigned width, unsigned height);
      void build_semantic_texture(VkDescriptorSet set, uint8_t *buffer,
            slang_texture_semantic semantic, const Texture &texture);
      void build_semantic_texture_array(VkDescriptorSet set, uint8_t *buffer,
            slang_texture_semantic semantic, unsigned index, const Texture &texture);
};

// struct here since we're implementing the opaque typedef from C.
struct vulkan_filter_chain
{
   public:
      vulkan_filter_chain(const vulkan_filter_chain_create_info &info);
      ~vulkan_filter_chain();

      inline void set_shader_preset(unique_ptr<video_shader> shader)
      {
         shader_preset = move(shader);
      }

      inline video_shader *get_shader_preset()
      {
         return shader_preset.get();
      }

      void set_pass_info(unsigned pass,
            const vulkan_filter_chain_pass_info &info);
      void set_shader(unsigned pass, VkShaderStageFlags stage,
            const uint32_t *spirv, size_t spirv_words);

      bool init();
      bool update_swapchain_info(
            const vulkan_filter_chain_swapchain_info &info);

      void notify_sync_index(unsigned index);
      void set_input_texture(const vulkan_filter_chain_texture &texture);
      void build_offscreen_passes(VkCommandBuffer cmd, const VkViewport &vp);
      void build_viewport_pass(VkCommandBuffer cmd,
            const VkViewport &vp, const float *mvp);

   private:
      VkDevice device;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkPipelineCache cache;
      vector<unique_ptr<Pass>> passes;
      vector<vulkan_filter_chain_pass_info> pass_info;
      vector<vector<function<void ()>>> deferred_calls;
      CommonResources common;
      VkFormat original_format;

      vulkan_filter_chain_texture input_texture;

      Size2D max_input_size;
      vulkan_filter_chain_swapchain_info swapchain_info;
      unsigned current_sync_index;

      unique_ptr<video_shader> shader_preset;

      void flush();

      void set_num_passes(unsigned passes);
      void execute_deferred();
      void set_num_sync_indices(unsigned num_indices);
      void set_swapchain_info(const vulkan_filter_chain_swapchain_info &info);

      bool init_history();
      bool init_feedback();
      void update_history(DeferredDisposer &disposer, VkCommandBuffer cmd);
      vector<unique_ptr<Framebuffer>> original_history;
      bool require_clear = false;
      void clear_history_and_feedback(VkCommandBuffer cmd);
      void update_feedback_info();
      void update_history_info();
};

vulkan_filter_chain::vulkan_filter_chain(
      const vulkan_filter_chain_create_info &info)
   : device(info.device),
     memory_properties(*info.memory_properties),
     cache(info.pipeline_cache),
     common(info.device, *info.memory_properties),
     original_format(info.original_format)
{
   max_input_size = { info.max_input_size.width, info.max_input_size.height };
   set_swapchain_info(info.swapchain);
   set_num_passes(info.num_passes);
}

vulkan_filter_chain::~vulkan_filter_chain()
{
   flush();
}

void vulkan_filter_chain::set_swapchain_info(
      const vulkan_filter_chain_swapchain_info &info)
{
   swapchain_info = info;
   set_num_sync_indices(info.num_indices);
}

void vulkan_filter_chain::set_num_sync_indices(unsigned num_indices)
{
   execute_deferred();
   deferred_calls.resize(num_indices);
}

void vulkan_filter_chain::notify_sync_index(unsigned index)
{
   auto &calls = deferred_calls[index];
   for (auto &call : calls)
      call();
   calls.clear();

   current_sync_index = index;

   for (auto &pass : passes)
      pass->notify_sync_index(index);
}

void vulkan_filter_chain::set_num_passes(unsigned num_passes)
{
   pass_info.resize(num_passes);
   passes.reserve(num_passes);
   for (unsigned i = 0; i < num_passes; i++)
   {
      passes.emplace_back(new Pass(device, memory_properties,
               cache, deferred_calls.size(), i + 1 == num_passes));
      passes.back()->set_common_resources(common);
   }
}

bool vulkan_filter_chain::update_swapchain_info(
      const vulkan_filter_chain_swapchain_info &info)
{
   flush();
   set_swapchain_info(info);
   return init();
}

void vulkan_filter_chain::set_pass_info(unsigned pass,
      const vulkan_filter_chain_pass_info &info)
{
   pass_info[pass] = info;
}

void vulkan_filter_chain::set_shader(
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   passes[pass]->set_shader(stage, spirv, spirv_words);
}

void vulkan_filter_chain::set_input_texture(
      const vulkan_filter_chain_texture &texture)
{
   input_texture = texture;
}

void vulkan_filter_chain::execute_deferred()
{
   for (auto &calls : deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
}

void vulkan_filter_chain::flush()
{
   VKFUNC(vkDeviceWaitIdle)(device);
   execute_deferred();
}

void vulkan_filter_chain::update_history_info()
{
   unsigned i = 0;
   for (auto &texture : original_history)
   {
      Texture &source         = common.original_history[i];
      source.texture.image    = texture->get_image();
      source.texture.view     = texture->get_view();
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = texture->get_size().width;
      source.texture.height   = texture->get_size().height;
      source.filter           = passes.front()->get_source_filter();
      i++;
   }
}

void vulkan_filter_chain::update_feedback_info()
{
   if (common.framebuffer_feedback.empty())
      return;

   for (unsigned i = 0; i < passes.size() - 1; i++)
   {
      auto fb = passes[i]->get_feedback_framebuffer();
      if (!fb)
         continue;

      auto &source = common.framebuffer_feedback[i];
      source.texture.image    = fb->get_image();
      source.texture.view     = fb->get_view();
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = fb->get_size().width;
      source.texture.height   = fb->get_size().height;
      source.filter           = passes[i]->get_source_filter();
   }
}

bool vulkan_filter_chain::init_history()
{
   original_history.clear();
   common.original_history.clear();

   require_clear = false;

   size_t required_images = 0;
   for (auto &pass : passes)
   {
      required_images =
         max(required_images,
               pass->get_reflection().semantic_textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size());
   }

   if (required_images < 2)
   {
      RARCH_LOG("[Vulkan filter chain]: Not using frame history.\n");
      return true;
   }

   // We don't need to store array element #0, since it's aliased with the actual original.
   required_images--;
   original_history.reserve(required_images);
   common.original_history.resize(required_images);

   for (unsigned i = 0; i < required_images; i++)
   {
      original_history.emplace_back(new Framebuffer(device, memory_properties,
               max_input_size, original_format));
   }

   RARCH_LOG("[Vulkan filter chain]: Using history of %u frames.\n", required_images);

   // On first frame, we need to clear the textures to a known state, but we need
   // a command buffer for that, so just defer to first frame.
   require_clear = true;
   return true;
}

bool vulkan_filter_chain::init_feedback()
{
   common.framebuffer_feedback.clear();

   bool use_feedbacks = false;

   // Final pass cannot have feedback.
   for (unsigned i = 0; i < passes.size() - 1; i++)
   {
      bool use_feedback = false;
      for (auto &pass : passes)
      {
         auto &r = pass->get_reflection();
         auto &feedbacks = r.semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];
         if (i < feedbacks.size() && feedbacks[i].texture)
         {
            use_feedback = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback && !passes[i]->init_feedback())
         return false;

      if (use_feedback)
         RARCH_LOG("[Vulkan filter chain]: Using framebuffer feedback for pass #%u.\n", i);
   }

   if (!use_feedbacks)
   {
      RARCH_LOG("[Vulkan filter chain]: Not using framebuffer feedback.\n");
      return true;
   }

   common.framebuffer_feedback.resize(passes.size() - 1);
   return true;
}

bool vulkan_filter_chain::init()
{
   Size2D source = max_input_size;

   for (unsigned i = 0; i < passes.size(); i++)
   {
      auto &pass = passes[i];
      source = pass->set_pass_info(max_input_size,
            source, swapchain_info, pass_info[i]);
      if (!pass->build())
         return false;
   }

   if (!init_history())
      return false;

   if (!init_feedback())
      return false;

   return true;
}

void vulkan_filter_chain::clear_history_and_feedback(VkCommandBuffer cmd)
{
   for (auto &texture : original_history)
      texture->clear(cmd);
   for (auto &pass : passes)
   {
      auto *fb = pass->get_feedback_framebuffer();
      if (fb)
         fb->clear(cmd);
   }
}

void vulkan_filter_chain::build_offscreen_passes(VkCommandBuffer cmd,
      const VkViewport &vp)
{
   // First frame, make sure our history and feedback textures are in a clean state.
   if (require_clear)
   {
      clear_history_and_feedback(cmd);
      require_clear = false;
   }

   update_history_info();
   update_feedback_info();

   unsigned i;
   DeferredDisposer disposer(deferred_calls[current_sync_index]);
   const Texture original = { 
      input_texture, passes.front()->get_source_filter() };
   Texture source         = { 
      input_texture, passes.front()->get_source_filter() };

   for (i = 0; i < passes.size() - 1; i++)
   {
      passes[i]->build_commands(disposer, cmd,
            original, source, vp, nullptr);

      auto &fb = passes[i]->get_framebuffer();
      source.texture.view     = fb.get_view();
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = fb.get_size().width;
      source.texture.height   = fb.get_size().height;
      source.filter           = passes[i + 1]->get_source_filter();
   }
}

void vulkan_filter_chain::update_history(DeferredDisposer &disposer, VkCommandBuffer cmd)
{
   VkImageLayout src_layout = input_texture.layout;

   // Transition input texture to something appropriate.
   if (input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      image_layout_transition(cmd,
            input_texture.image,
            input_texture.layout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

      src_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   }

   unique_ptr<Framebuffer> tmp;
   unique_ptr<Framebuffer> &back = original_history.back();
   swap(back, tmp);

   if (input_texture.width != tmp->get_size().width ||
         input_texture.height != tmp->get_size().height)
   {
      tmp->set_size(disposer, { input_texture.width, input_texture.height });
   }

   tmp->copy(cmd, input_texture.image, src_layout);

   // Transition input texture back.
   if (input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      image_layout_transition(cmd,
            input_texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            input_texture.layout,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   }

   // Should ring buffer, but we don't have *that* many passes.
   move_backward(begin(original_history), end(original_history) - 1, end(original_history));
   swap(original_history.front(), tmp);
}

void vulkan_filter_chain::build_viewport_pass(
      VkCommandBuffer cmd, const VkViewport &vp, const float *mvp)
{
   // First frame, make sure our history and feedback textures are in a clean state.
   if (require_clear)
   {
      clear_history_and_feedback(cmd);
      require_clear = false;
   }

   Texture source;
   DeferredDisposer disposer(deferred_calls[current_sync_index]);
   const Texture original = { 
      input_texture, passes.front()->get_source_filter() };

   if (passes.size() == 1)
      source = { input_texture, passes.back()->get_source_filter() };
   else
   {
      auto &fb = passes[passes.size() - 2]->get_framebuffer();
      source.texture.view    = fb.get_view();
      source.texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width   = fb.get_size().width;
      source.texture.height  = fb.get_size().height;
      source.filter          = passes.back()->get_source_filter();
   }

   passes.back()->build_commands(disposer, cmd,
         original, source, vp, mvp);

   // If we need to keep old frames, copy it after fragment is complete.
   // TODO: We can improve pipelining by figuring out which pass is the last that reads from
   // the history and dispatch the copy earlier.
   if (!original_history.empty())
      update_history(disposer, cmd);

   // For feedback FBOs, swap current and previous.
   for (auto &pass : passes)
      pass->end_frame();
}

Buffer::Buffer(VkDevice device,
      const VkPhysicalDeviceMemoryProperties &mem_props,
      size_t size, VkBufferUsageFlags usage) :
   device(device), size(size)
{
   VkMemoryRequirements mem_reqs;
   VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
   info.size               = size;
   info.usage              = usage;
   info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
   VKFUNC(vkCreateBuffer)(device, &info, nullptr, &buffer);

   VKFUNC(vkGetBufferMemoryRequirements)(device, buffer, &mem_reqs);

   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   alloc.allocationSize       = mem_reqs.size;

   alloc.memoryTypeIndex      = find_memory_type(
         mem_props, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   VKFUNC(vkAllocateMemory)(device, &alloc, NULL, &memory);
   VKFUNC(vkBindBufferMemory)(device, buffer, memory, 0);
}

void *Buffer::map()
{
   void *ptr = nullptr;
   if (VKFUNC(vkMapMemory)(device, memory, 0, size, 0, &ptr) == VK_SUCCESS)
      return ptr;
   return nullptr;
}

void Buffer::unmap()
{
   VKFUNC(vkUnmapMemory)(device, memory);
}

Buffer::~Buffer()
{
   if (memory != VK_NULL_HANDLE)
      VKFUNC(vkFreeMemory)(device, memory, nullptr);
   if (buffer != VK_NULL_HANDLE)
      VKFUNC(vkDestroyBuffer)(device, buffer, nullptr);
}

Pass::~Pass()
{
   clear_vk();
}

void Pass::set_shader(VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   if (stage == VK_SHADER_STAGE_VERTEX_BIT)
   {
      vertex_shader.clear();
      vertex_shader.insert(end(vertex_shader),
            spirv, spirv + spirv_words);
   }
   else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT)
   {
      fragment_shader.clear();
      fragment_shader.insert(end(fragment_shader),
            spirv, spirv + spirv_words);
   }
}

Size2D Pass::get_output_size(const Size2D &original,
      const Size2D &source) const
{
   float width, height;
   switch (pass_info.scale_type_x)
   {
      case VULKAN_FILTER_CHAIN_SCALE_ORIGINAL:
         width = float(original.width) * pass_info.scale_x;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_SOURCE:
         width = float(source.width) * pass_info.scale_x;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_VIEWPORT:
         width = current_viewport.width * pass_info.scale_x;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_ABSOLUTE:
         width = pass_info.scale_x;
         break;

      default:
         width = 0.0f;
   }

   switch (pass_info.scale_type_y)
   {
      case VULKAN_FILTER_CHAIN_SCALE_ORIGINAL:
         height = float(original.height) * pass_info.scale_y;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_SOURCE:
         height = float(source.height) * pass_info.scale_y;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_VIEWPORT:
         height = current_viewport.height * pass_info.scale_y;
         break;

      case VULKAN_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass_info.scale_y;
         break;

      default:
         height = 0.0f;
   }

   return { unsigned(roundf(width)), unsigned(roundf(height)) };
}

Size2D Pass::set_pass_info(
      const Size2D &max_original,
      const Size2D &max_source,
      const vulkan_filter_chain_swapchain_info &swapchain,
      const vulkan_filter_chain_pass_info &info)
{
   clear_vk();

   current_viewport = swapchain.viewport;
   pass_info = info;

   num_sync_indices = swapchain.num_indices;
   sync_index = 0;

   current_framebuffer_size = get_output_size(max_original, max_source);
   swapchain_render_pass = swapchain.render_pass;
   return current_framebuffer_size;
}

void Pass::clear_vk()
{
   if (pool != VK_NULL_HANDLE)
      VKFUNC(vkDestroyDescriptorPool)(device, pool, nullptr);
   if (pipeline != VK_NULL_HANDLE)
      VKFUNC(vkDestroyPipeline)(device, pipeline, nullptr);
   if (set_layout != VK_NULL_HANDLE)
      VKFUNC(vkDestroyDescriptorSetLayout)(device, set_layout, nullptr);
   if (pipeline_layout != VK_NULL_HANDLE)
      VKFUNC(vkDestroyPipelineLayout)(device, pipeline_layout, nullptr);

   pool       = VK_NULL_HANDLE;
   pipeline   = VK_NULL_HANDLE;
   set_layout = VK_NULL_HANDLE;
   ubos.clear();
}

bool Pass::init_pipeline_layout()
{
   vector<VkDescriptorSetLayoutBinding> bindings;
   vector<VkDescriptorPoolSize> desc_counts;

   // Main UBO.
   VkShaderStageFlags ubo_mask = 0;
   if (reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK)
      ubo_mask |= VK_SHADER_STAGE_VERTEX_BIT;
   if (reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
      ubo_mask |= VK_SHADER_STAGE_FRAGMENT_BIT;

   if (ubo_mask != 0)
   {
      bindings.push_back({ reflection.ubo_binding,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            ubo_mask, nullptr });
      desc_counts.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_sync_indices });
   }

   // Semantic textures.
   for (auto &semantic : reflection.semantic_textures)
   {
      for (auto &texture : semantic)
      {
         if (!texture.texture)
            continue;

         VkShaderStageFlags stages = 0;
         if (texture.stage_mask & SLANG_STAGE_VERTEX_MASK)
            stages |= VK_SHADER_STAGE_VERTEX_BIT;
         if (texture.stage_mask & SLANG_STAGE_FRAGMENT_MASK)
            stages |= VK_SHADER_STAGE_FRAGMENT_BIT;

         bindings.push_back({ texture.binding,
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
               stages, nullptr });
         desc_counts.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_sync_indices });
      }
   }

   VkDescriptorSetLayoutCreateInfo set_layout_info = { 
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
   set_layout_info.bindingCount = bindings.size();
   set_layout_info.pBindings    = bindings.data();

   if (VKFUNC(vkCreateDescriptorSetLayout)(device,
            &set_layout_info, NULL, &set_layout) != VK_SUCCESS)
      return false;

   VkPipelineLayoutCreateInfo layout_info = { 
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
   layout_info.setLayoutCount             = 1;
   layout_info.pSetLayouts                = &set_layout;

   if (VKFUNC(vkCreatePipelineLayout)(device,
            &layout_info, NULL, &pipeline_layout) != VK_SUCCESS)
      return false;

   VkDescriptorPoolCreateInfo pool_info = { 
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   pool_info.maxSets                    = num_sync_indices;
   pool_info.poolSizeCount              = desc_counts.size();
   pool_info.pPoolSizes                 = desc_counts.data();
   if (VKFUNC(vkCreateDescriptorPool)(device, &pool_info, nullptr, &pool) != VK_SUCCESS)
      return false;

   VkDescriptorSetAllocateInfo alloc_info = { 
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
   alloc_info.descriptorPool     = pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts        = &set_layout;

   sets.resize(num_sync_indices);

   for (unsigned i = 0; i < num_sync_indices; i++)
      VKFUNC(vkAllocateDescriptorSets)(device, &alloc_info, &sets[i]);

   return true;
}

bool Pass::init_pipeline()
{
   if (!init_pipeline_layout())
      return false;

   // Input assembly
   VkPipelineInputAssemblyStateCreateInfo input_assembly = { 
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

   // VAO state
   VkVertexInputAttributeDescription attributes[2] = {{0}};
   VkVertexInputBindingDescription binding = {0};

   attributes[0].location = 0;
   attributes[0].binding  = 0;
   attributes[0].format   = VK_FORMAT_R32G32_SFLOAT;
   attributes[0].offset   = 0;
   attributes[1].location = 1;
   attributes[1].binding  = 0;
   attributes[1].format   = VK_FORMAT_R32G32_SFLOAT;
   attributes[1].offset   = 2 * sizeof(float);

   binding.binding        = 0;
   binding.stride         = 4 * sizeof(float);
   binding.inputRate      = VK_VERTEX_INPUT_RATE_VERTEX;

   VkPipelineVertexInputStateCreateInfo vertex_input = { 
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
   vertex_input.vertexBindingDescriptionCount   = 1;
   vertex_input.pVertexBindingDescriptions      = &binding;
   vertex_input.vertexAttributeDescriptionCount = 2;
   vertex_input.pVertexAttributeDescriptions    = attributes;

   // Raster state
   VkPipelineRasterizationStateCreateInfo raster = { 
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
   raster.polygonMode = VK_POLYGON_MODE_FILL;
   raster.cullMode = VK_CULL_MODE_NONE;
   raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.depthClampEnable = false;
   raster.rasterizerDiscardEnable = false;
   raster.depthBiasEnable = false;
   raster.lineWidth = 1.0f;

   // Blend state
   VkPipelineColorBlendAttachmentState blend_attachment = {0};
   VkPipelineColorBlendStateCreateInfo blend = { 
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
   blend_attachment.blendEnable    = false;
   blend_attachment.colorWriteMask = 0xf;
   blend.attachmentCount           = 1;
   blend.pAttachments              = &blend_attachment;

   // Viewport state
   VkPipelineViewportStateCreateInfo viewport = { 
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
   viewport.viewportCount = 1;
   viewport.scissorCount  = 1;

   // Depth-stencil state
   VkPipelineDepthStencilStateCreateInfo depth_stencil = { 
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
   depth_stencil.depthTestEnable       = false;
   depth_stencil.depthWriteEnable      = false;
   depth_stencil.depthBoundsTestEnable = false;
   depth_stencil.stencilTestEnable     = false;
   depth_stencil.minDepthBounds        = 0.0f;
   depth_stencil.maxDepthBounds        = 1.0f;

   // Multisample state
   VkPipelineMultisampleStateCreateInfo multisample = { 
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
   multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   // Dynamic state
   VkPipelineDynamicStateCreateInfo dynamic = { 
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
   static const VkDynamicState dynamics[] = { 
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   dynamic.pDynamicStates    = dynamics;
   dynamic.dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]);

   // Shaders
   VkPipelineShaderStageCreateInfo shader_stages[2] = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
   };

   VkShaderModuleCreateInfo module_info = { 
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
   module_info.codeSize     = vertex_shader.size() * sizeof(uint32_t);
   module_info.pCode        = vertex_shader.data();
   shader_stages[0].stage   = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName   = "main";
   VKFUNC(vkCreateShaderModule)(device, &module_info, NULL, &shader_stages[0].module);

   module_info.codeSize     = fragment_shader.size() * sizeof(uint32_t);
   module_info.pCode        = fragment_shader.data();
   shader_stages[1].stage   = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName   = "main";
   VKFUNC(vkCreateShaderModule)(device, &module_info, NULL, &shader_stages[1].module);

   VkGraphicsPipelineCreateInfo pipe = { 
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
   pipe.stageCount          = 2;
   pipe.pStages             = shader_stages;
   pipe.pVertexInputState   = &vertex_input;
   pipe.pInputAssemblyState = &input_assembly;
   pipe.pRasterizationState = &raster;
   pipe.pColorBlendState    = &blend;
   pipe.pMultisampleState   = &multisample;
   pipe.pViewportState      = &viewport;
   pipe.pDepthStencilState  = &depth_stencil;
   pipe.pDynamicState       = &dynamic;
   pipe.renderPass          = final_pass ? swapchain_render_pass : 
      framebuffer->get_render_pass();
   pipe.layout              = pipeline_layout;

   if (VKFUNC(vkCreateGraphicsPipelines)(device,
            cache, 1, &pipe, NULL, &pipeline) != VK_SUCCESS)
   {
      VKFUNC(vkDestroyShaderModule)(device, shader_stages[0].module, NULL);
      VKFUNC(vkDestroyShaderModule)(device, shader_stages[1].module, NULL);
      return false;
   }

   VKFUNC(vkDestroyShaderModule)(device, shader_stages[0].module, NULL);
   VKFUNC(vkDestroyShaderModule)(device, shader_stages[1].module, NULL);
   return true;
}

CommonResources::CommonResources(VkDevice device,
      const VkPhysicalDeviceMemoryProperties &memory_properties)
   : device(device)
{
   // The final pass uses an MVP designed for [0, 1] range VBO.
   // For in-between passes, we just go with identity matrices, so keep it simple.
   const float vbo_data_offscreen[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,
      -1.0f, +1.0f, 0.0f, 1.0f,
       1.0f, -1.0f, 1.0f, 0.0f,
       1.0f, +1.0f, 1.0f, 1.0f,
   };

   const float vbo_data_final[] = {
      0.0f,  0.0f, 0.0f, 0.0f,
      0.0f, +1.0f, 0.0f, 1.0f,
      1.0f,  0.0f, 1.0f, 0.0f,
      1.0f, +1.0f, 1.0f, 1.0f,
   };

   vbo_offscreen = unique_ptr<Buffer>(new Buffer(device,
            memory_properties, sizeof(vbo_data_offscreen), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

   void *ptr = vbo_offscreen->map();
   memcpy(ptr, vbo_data_offscreen, sizeof(vbo_data_offscreen));
   vbo_offscreen->unmap();

   vbo_final = unique_ptr<Buffer>(new Buffer(device,
            memory_properties, sizeof(vbo_data_final), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

   ptr = vbo_final->map();
   memcpy(ptr, vbo_data_final, sizeof(vbo_data_final));
   vbo_final->unmap();

   VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.mipLodBias              = 0.0f;
   info.maxAnisotropy           = 1.0f;
   info.compareEnable           = false;
   info.minLod                  = 0.0f;
   info.maxLod                  = 0.0f;
   info.unnormalizedCoordinates = false;
   info.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

   VKFUNC(vkCreateSampler)(device,
            &info, nullptr, &samplers[VULKAN_FILTER_CHAIN_NEAREST]);

   info.magFilter = VK_FILTER_LINEAR;
   info.minFilter = VK_FILTER_LINEAR;

   VKFUNC(vkCreateSampler)(device,
            &info, nullptr, &samplers[VULKAN_FILTER_CHAIN_LINEAR]);
}

CommonResources::~CommonResources()
{
   for (auto &samp : samplers)
      if (samp != VK_NULL_HANDLE)
         VKFUNC(vkDestroySampler)(device, samp, nullptr);
}

bool Pass::init_buffers()
{
   ubos.clear();
   if (reflection.ubo_stage_mask)
   {
      for (unsigned i = 0; i < num_sync_indices; i++)
         ubos.emplace_back(new Buffer(device,
                  memory_properties, reflection.ubo_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
   }
   return true;
}

void Pass::end_frame()
{
   if (framebuffer_feedback)
      swap(framebuffer, framebuffer_feedback);
}

bool Pass::init_feedback()
{
   if (final_pass)
      return false;

   framebuffer_feedback = unique_ptr<Framebuffer>(
         new Framebuffer(device, memory_properties,
            current_framebuffer_size,
            pass_info.rt_format));
   return true;
}

bool Pass::build()
{
   framebuffer.reset();
   framebuffer_feedback.reset();

   if (!final_pass)
   {
      framebuffer = unique_ptr<Framebuffer>(
            new Framebuffer(device, memory_properties,
               current_framebuffer_size,
               pass_info.rt_format));
   }

   reflection = slang_reflection{};
   if (!slang_reflect_spirv(vertex_shader, fragment_shader, &reflection))
      return false;

   if (!init_pipeline())
      return false;

   if (!init_buffers())
      return false;

   return true;
}

void Pass::set_uniform_buffer(VkDescriptorSet set, unsigned binding,
      VkBuffer buffer,
      VkDeviceSize offset,
      VkDeviceSize range)
{
   VkDescriptorBufferInfo buffer_info;
   buffer_info.buffer = buffer;
   buffer_info.offset = offset;
   buffer_info.range = range;

   VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
   write.dstSet               = set;
   write.dstBinding           = binding;
   write.descriptorCount      = 1;
   write.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   write.pBufferInfo = &buffer_info;

   VKFUNC(vkUpdateDescriptorSets)(device, 1, &write, 0, NULL);
}

void Pass::set_texture(VkDescriptorSet set, unsigned binding,
      const Texture &texture)
{
   VkDescriptorImageInfo image_info;
   image_info.sampler         = common->samplers[texture.filter];
   image_info.imageView       = texture.texture.view;
   image_info.imageLayout     = texture.texture.layout;

   VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
   write.dstSet               = set;
   write.dstBinding           = binding;
   write.descriptorCount      = 1;
   write.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   write.pImageInfo           = &image_info;

   VKFUNC(vkUpdateDescriptorSets)(device, 1, &write, 0, nullptr);
}

void Pass::set_semantic_texture(VkDescriptorSet set,
      slang_texture_semantic semantic, const Texture &texture)
{
   if (reflection.semantic_textures[semantic][0].texture)
      set_texture(set, reflection.semantic_textures[semantic][0].binding, texture);
}

void Pass::set_semantic_texture_array(VkDescriptorSet set,
      slang_texture_semantic semantic, unsigned index,
      const Texture &texture)
{
   if (index < reflection.semantic_textures[semantic].size() &&
         reflection.semantic_textures[semantic][index].texture)
   {
      set_texture(set, reflection.semantic_textures[semantic][index].binding, texture);
   }
}

void Pass::build_semantic_texture_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned width, unsigned height)
{
   if (data && reflection.semantic_textures[semantic][0].uniform)
   {
      build_vec4(
            reinterpret_cast<float *>(data + reflection.semantic_textures[semantic][0].ubo_offset),
            width,
            height);
   }
}

void Pass::build_semantic_texture_array_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height)
{
   if (data && index < reflection.semantic_textures[semantic].size() &&
         reflection.semantic_textures[semantic][index].uniform)
   {
      build_vec4(
            reinterpret_cast<float *>(data + reflection.semantic_textures[semantic][index].ubo_offset),
            width,
            height);
   }
}

void Pass::build_semantic_vec4(uint8_t *data, slang_semantic semantic,
      unsigned width, unsigned height)
{
   if (data && reflection.semantics[semantic].uniform)
   {
      build_vec4(
            reinterpret_cast<float *>(data + reflection.semantics[semantic].ubo_offset),
            width,
            height);
   }
}

void Pass::build_semantic_texture(VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, const Texture &texture)
{
   build_semantic_texture_vec4(buffer, semantic,
         texture.texture.width, texture.texture.height);
   set_semantic_texture(set, semantic, texture);
}

void Pass::build_semantic_texture_array(VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, unsigned index, const Texture &texture)
{
   build_semantic_texture_array_vec4(buffer, semantic, index,
         texture.texture.width, texture.texture.height);
   set_semantic_texture_array(set, semantic, index, texture);
}

void Pass::build_semantics(VkDescriptorSet set, uint8_t *buffer,
      const float *mvp, const Texture &original, const Texture &source)
{
   // MVP
   if (buffer && reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = reflection.semantics[SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(buffer + offset, mvp, sizeof(float) * 16);
      else
         build_identity_matrix(reinterpret_cast<float *>(buffer + offset));
   }

   // Output information
   build_semantic_vec4(buffer, SLANG_SEMANTIC_OUTPUT,
         current_framebuffer_size.width, current_framebuffer_size.height);
   build_semantic_vec4(buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
         unsigned(current_viewport.width), unsigned(current_viewport.height));

   // Standard inputs
   build_semantic_texture(set, buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   build_semantic_texture(set, buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   // ORIGINAL_HISTORY[0] is an alias of ORIGINAL.
   build_semantic_texture_array(set, buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   // Previous inputs.
   unsigned i = 0;
   for (auto &texture : common->original_history)
   {
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            texture);
      i++;
   }

   // Feedback FBOs.
   i = 0;
   for (auto &texture : common->framebuffer_feedback)
   {
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            texture);
      i++;
   }
}

void Pass::build_commands(
      DeferredDisposer &disposer,
      VkCommandBuffer cmd,
      const Texture &original,
      const Texture &source,
      const VkViewport &vp,
      const float *mvp)
{
   current_viewport = vp;
   auto size = get_output_size(
         { original.texture.width, original.texture.height },
         { source.texture.width, source.texture.height });

   if (     size.width  != current_framebuffer_size.width 
         || size.height != current_framebuffer_size.height)
   {
      if (framebuffer)
         framebuffer->set_size(disposer, size);
      current_framebuffer_size = size;
   }

   if (reflection.ubo_stage_mask)
   {
      uint8_t *u = static_cast<uint8_t*>(ubos[sync_index]->map());
      build_semantics(sets[sync_index], u, mvp, original, source);
      ubos[sync_index]->unmap();
   }
   else
      build_semantics(sets[sync_index], nullptr, mvp, original, source);

   if (reflection.ubo_stage_mask)
   {
      set_uniform_buffer(sets[sync_index], 0,
            ubos[sync_index]->get_buffer(), 0, reflection.ubo_size);
   }

   // The final pass is always executed inside 
   // another render pass since the frontend will 
   // want to overlay various things on top for 
   // the passes that end up on-screen.
   if (!final_pass)
   {
      // Render.
      image_layout_transition(cmd,
            framebuffer->get_image(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

      VkRenderPassBeginInfo rp_info = { 
         VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
      VkClearValue clear_value;
      clear_value.color.float32[0]     = 0.0f;
      clear_value.color.float32[1]     = 0.0f;
      clear_value.color.float32[2]     = 0.0f;
      clear_value.color.float32[3]     = 1.0f;
      rp_info.renderPass               = framebuffer->get_render_pass();
      rp_info.framebuffer              = framebuffer->get_framebuffer();
      rp_info.renderArea.extent.width  = current_framebuffer_size.width;
      rp_info.renderArea.extent.height = current_framebuffer_size.height;
      rp_info.clearValueCount          = 1;
      rp_info.pClearValues             = &clear_value;

      VKFUNC(vkCmdBeginRenderPass)(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
   }

   VKFUNC(vkCmdBindPipeline)(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
   VKFUNC(vkCmdBindDescriptorSets)(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
         0, 1, &sets[sync_index], 0, nullptr);

   VkDeviceSize offset = 0;
   VKFUNC(vkCmdBindVertexBuffers)(cmd, 0, 1,
         final_pass ? &common->vbo_final->get_buffer() : &common->vbo_offscreen->get_buffer(),
         &offset);

   if (final_pass)
   {
      const VkRect2D sci = {
         {
            int32_t(current_viewport.x),
            int32_t(current_viewport.y)
         },
         {
            uint32_t(current_viewport.width),
            uint32_t(current_viewport.height)
         },
      };
      VKFUNC(vkCmdSetViewport)(cmd, 0, 1, &current_viewport);
      VKFUNC(vkCmdSetScissor)(cmd, 0, 1, &sci);
   }
   else
   {
      const VkViewport vp = {
         0.0f, 0.0f,
         float(current_framebuffer_size.width),
         float(current_framebuffer_size.height),
         0.0f, 1.0f
      };
      const VkRect2D sci = {
         { 0, 0 },
         { 
            current_framebuffer_size.width,
            current_framebuffer_size.height
         },
      };

      VKFUNC(vkCmdSetViewport)(cmd, 0, 1, &vp);
      VKFUNC(vkCmdSetScissor)(cmd, 0, 1, &sci);
   }

   VKFUNC(vkCmdDraw)(cmd, 4, 1, 0, 0);

   if (!final_pass)
   {
      VKFUNC(vkCmdEndRenderPass)(cmd);

      // Barrier to sync with next pass.
      image_layout_transition(
            cmd,
            framebuffer->get_image(),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   }
}

Framebuffer::Framebuffer(
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties &mem_props,
      const Size2D &max_size, VkFormat format) :
   device(device),
   memory_properties(mem_props),
   size(max_size),
   format(format)
{
   RARCH_LOG("[Vulkan filter chain]: Creating framebuffer %u x %u.\n",
         max_size.width, max_size.height);
   init_render_pass();
   init(nullptr);
}

void Framebuffer::clear(VkCommandBuffer cmd)
{
   image_layout_transition(cmd, image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   VkClearColorValue color;
   memset(&color, 0, sizeof(color));

   VkImageSubresourceRange range;
   memset(&range, 0, sizeof(range));
   range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   range.levelCount = 1;
   range.layerCount = 1;

   VKFUNC(vkCmdClearColorImage)(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &color, 1, &range);

   image_layout_transition(cmd, image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void Framebuffer::copy(VkCommandBuffer cmd,
      VkImage src_image, VkImageLayout src_layout)
{
   image_layout_transition(cmd, image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   VkImageCopy region;
   memset(&region, 0, sizeof(region));
   region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.srcSubresource.layerCount = 1;
   region.dstSubresource = region.srcSubresource;
   region.extent.width = size.width;
   region.extent.height = size.height;
   region.extent.depth = 1;

   VKFUNC(vkCmdCopyImage)(cmd,
         src_image, src_layout,
         image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   image_layout_transition(cmd, image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void Framebuffer::init(DeferredDisposer *disposer)
{
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
   info.imageType         = VK_IMAGE_TYPE_2D;
   info.format            = format;
   info.extent.width      = size.width;
   info.extent.height     = size.height;
   info.extent.depth      = 1;
   info.mipLevels         = 1;
   info.arrayLayers       = 1;
   info.samples           = VK_SAMPLE_COUNT_1_BIT;
   info.tiling            = VK_IMAGE_TILING_OPTIMAL;
   info.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | 
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   info.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

   VKFUNC(vkCreateImage)(device, &info, nullptr, &image);

   VKFUNC(vkGetImageMemoryRequirements)(device, image, &mem_reqs);

   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   alloc.allocationSize   = mem_reqs.size;
   alloc.memoryTypeIndex  = find_memory_type_fallback(
         memory_properties, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   // Can reuse already allocated memory.
   if (memory.size < mem_reqs.size || memory.type != alloc.memoryTypeIndex)
   {
      // Memory might still be in use since we don't want to totally stall
      // the world for framebuffer recreation.
      if (memory.memory != VK_NULL_HANDLE && disposer)
      {
         auto d = device;
         auto m = memory.memory;
         disposer->defer([=] { VKFUNC(vkFreeMemory)(d, m, nullptr); });
      }

      memory.type = alloc.memoryTypeIndex;
      memory.size = mem_reqs.size;

      VKFUNC(vkAllocateMemory)(device, &alloc, nullptr, &memory.memory);
   }

   VKFUNC(vkBindImageMemory)(device, image, memory.memory, 0);

   VkImageViewCreateInfo view_info           = { 
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
   view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                          = format;
   view_info.image                           = image;
   view_info.subresourceRange.baseMipLevel   = 0;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.levelCount     = 1;
   view_info.subresourceRange.layerCount     = 1;
   view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;

   VKFUNC(vkCreateImageView)(device, &view_info, nullptr, &view);

   init_framebuffer();
}

void Framebuffer::init_render_pass()
{
   VkRenderPassCreateInfo rp_info = { 
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
   VkAttachmentReference color_ref = { 0, 
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

   // We will always write to the entire framebuffer,
   // so we don't really need to clear.
   VkAttachmentDescription attachment = {0};
   attachment.format            = format;
   attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
   attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
   attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;

   attachment.initialLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   attachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {0};
   subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments    = &color_ref;

   rp_info.attachmentCount      = 1;
   rp_info.pAttachments         = &attachment;
   rp_info.subpassCount         = 1;
   rp_info.pSubpasses           = &subpass;

   VKFUNC(vkCreateRenderPass)(device, &rp_info, nullptr, &render_pass);
}

void Framebuffer::init_framebuffer()
{
   VkFramebufferCreateInfo info = { 
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
   info.renderPass      = render_pass;
   info.attachmentCount = 1;
   info.pAttachments    = &view;
   info.width           = size.width;
   info.height          = size.height;
   info.layers          = 1;

   VKFUNC(vkCreateFramebuffer)(device, &info, nullptr, &framebuffer);
}

void Framebuffer::set_size(DeferredDisposer &disposer, const Size2D &size)
{
   this->size = size;

   RARCH_LOG("[Vulkan filter chain]: Updating framebuffer size %u x %u.\n",
         size.width, size.height);

   {
      // The current framebuffers, etc, might still be in use
      // so defer deletion.
      // We'll most likely be able to reuse the memory,
      // so don't free it here.
      //
      // Fake lambda init captures for C++11.
      //
      auto d  = device;
      auto i  = image;
      auto v  = view;
      auto fb = framebuffer;
      disposer.defer([=]
      {
         if (fb != VK_NULL_HANDLE)
            VKFUNC(vkDestroyFramebuffer)(d, fb, nullptr);
         if (v != VK_NULL_HANDLE)
            VKFUNC(vkDestroyImageView)(d, v, nullptr);
         if (i != VK_NULL_HANDLE)
            VKFUNC(vkDestroyImage)(d, i, nullptr);
      });
   }

   init(&disposer);
}

Framebuffer::~Framebuffer()
{
   if (framebuffer != VK_NULL_HANDLE)
      VKFUNC(vkDestroyFramebuffer)(device, framebuffer, nullptr);
   if (render_pass != VK_NULL_HANDLE)
      VKFUNC(vkDestroyRenderPass)(device, render_pass, nullptr);
   if (view != VK_NULL_HANDLE)
      VKFUNC(vkDestroyImageView)(device, view, nullptr);
   if (image != VK_NULL_HANDLE)
      VKFUNC(vkDestroyImage)(device, image, nullptr);
   if (memory.memory != VK_NULL_HANDLE)
      VKFUNC(vkFreeMemory)(device, memory.memory, nullptr);
}

// C glue
vulkan_filter_chain_t *vulkan_filter_chain_new(
      const vulkan_filter_chain_create_info *info)
{
   return new vulkan_filter_chain(*info);
}

vulkan_filter_chain_t *vulkan_filter_chain_create_default(
      const struct vulkan_filter_chain_create_info *info,
      vulkan_filter_chain_filter filter)
{
   struct vulkan_filter_chain_pass_info pass_info;
   auto tmpinfo       = *info;
   tmpinfo.num_passes = 1;

   unique_ptr<vulkan_filter_chain> chain{ new vulkan_filter_chain(tmpinfo) };
   if (!chain)
      return nullptr;

   memset(&pass_info, 0, sizeof(pass_info));
   pass_info.scale_type_x  = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = tmpinfo.swapchain.format;
   pass_info.source_filter = filter;
   chain->set_pass_info(0, pass_info);

   chain->set_shader(0, VK_SHADER_STAGE_VERTEX_BIT,
         (const uint32_t*)opaque_vert_spv,
         opaque_vert_spv_len / sizeof(uint32_t));
   chain->set_shader(0, VK_SHADER_STAGE_FRAGMENT_BIT,
         (const uint32_t*)opaque_frag_spv,
         opaque_frag_spv_len / sizeof(uint32_t));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

struct ConfigDeleter
{
   void operator()(config_file_t *conf)
   {
      if (conf)
         config_file_free(conf);
   }
};

vulkan_filter_chain_t *vulkan_filter_chain_create_from_preset(
      const struct vulkan_filter_chain_create_info *info,
      const char *path, vulkan_filter_chain_filter filter)
{
   unique_ptr<video_shader> shader{ new video_shader() };
   if (!shader)
      return nullptr;

   unique_ptr<config_file_t, ConfigDeleter> conf{ config_file_new(path) };
   if (!path)
      return nullptr;

   if (!video_shader_read_conf_cgp(conf.get(), shader.get()))
      return nullptr;

   video_shader_resolve_relative(shader.get(), path);
   video_shader_resolve_parameters(conf.get(), shader.get());

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.valid;
   auto tmpinfo          = *info;
   tmpinfo.num_passes    = shader->passes + (last_pass_is_fbo ? 1 : 0);

   unique_ptr<vulkan_filter_chain> chain{ new vulkan_filter_chain(tmpinfo) };
   if (!chain)
      return nullptr;

   for (unsigned i = 0; i < shader->passes; i++)
   {
      const video_shader_pass *pass = &shader->pass[i];
      struct vulkan_filter_chain_pass_info pass_info;
      memset(&pass_info, 0, sizeof(pass_info));

      glslang_output output;
      if (!glslang_compile_shader(pass->source.path, &output))
      {
         RARCH_ERR("Failed to compile shader: \"%s\".\n",
               pass->source.path);
         return nullptr;
      }

      chain->set_shader(i,
            VK_SHADER_STAGE_VERTEX_BIT,
            output.vertex.data(),
            output.vertex.size());

      chain->set_shader(i,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            output.fragment.data(),
            output.fragment.size());

      if (pass->filter == RARCH_FILTER_UNSPEC)
         pass_info.source_filter = filter;
      else
      {
         pass_info.source_filter =
            pass->filter == RARCH_FILTER_LINEAR ? VULKAN_FILTER_CHAIN_LINEAR : 
            VULKAN_FILTER_CHAIN_NEAREST;
      }

      if (!pass->fbo.valid)
      {
         pass_info.scale_type_x = i + 1 == shader->passes 
            ? VULKAN_FILTER_CHAIN_SCALE_VIEWPORT 
            : VULKAN_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_type_y = i + 1 == shader->passes 
            ? VULKAN_FILTER_CHAIN_SCALE_VIEWPORT 
            : VULKAN_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_x = 1.0f;
         pass_info.scale_y = 1.0f;
         pass_info.rt_format = i + 1 == shader->passes 
            ? tmpinfo.swapchain.format 
            : VK_FORMAT_R8G8B8A8_UNORM;
      }
      else
      {
         // TODO: Add more general format spec.
         pass_info.rt_format = VK_FORMAT_R8G8B8A8_UNORM;
         if (pass->fbo.srgb_fbo)
            pass_info.rt_format = VK_FORMAT_R8G8B8A8_SRGB;
         else if (pass->fbo.fp_fbo)
            pass_info.rt_format = VK_FORMAT_R16G16B16A16_SFLOAT;

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_x = pass->fbo.scale_x;
               pass_info.scale_type_x = VULKAN_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_x = float(pass->fbo.abs_x);
               pass_info.scale_type_x = VULKAN_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_x = pass->fbo.scale_x;
               pass_info.scale_type_x = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_y = pass->fbo.scale_y;
               pass_info.scale_type_y = VULKAN_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_y = float(pass->fbo.abs_y);
               pass_info.scale_type_y = VULKAN_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_y = pass->fbo.scale_y;
               pass_info.scale_type_y = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }
      }

      chain->set_pass_info(i, pass_info);
   }

   if (last_pass_is_fbo)
   {
      struct vulkan_filter_chain_pass_info pass_info;
      memset(&pass_info, 0, sizeof(pass_info));
      pass_info.scale_type_x = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y = VULKAN_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x = 1.0f;
      pass_info.scale_y = 1.0f;
      pass_info.rt_format = tmpinfo.swapchain.format;
      pass_info.source_filter = filter;
      chain->set_pass_info(shader->passes, pass_info);

      chain->set_shader(shader->passes,
            VK_SHADER_STAGE_VERTEX_BIT,
            (const uint32_t*)opaque_vert_spv,
            opaque_vert_spv_len / sizeof(uint32_t));

      chain->set_shader(shader->passes,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            (const uint32_t*)opaque_frag_spv,
            opaque_frag_spv_len / sizeof(uint32_t));
   }

   chain->set_shader_preset(move(shader));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

struct video_shader *vulkan_filter_chain_get_preset(
      vulkan_filter_chain_t *chain)
{
   return chain->get_shader_preset();
}

void vulkan_filter_chain_free(
      vulkan_filter_chain_t *chain)
{
   delete chain;
}

void vulkan_filter_chain_set_shader(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   chain->set_shader(pass, stage, spirv, spirv_words);
}

void vulkan_filter_chain_set_pass_info(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      const struct vulkan_filter_chain_pass_info *info)
{
   chain->set_pass_info(pass, *info);
}

bool vulkan_filter_chain_update_swapchain_info(
      vulkan_filter_chain_t *chain,
      const vulkan_filter_chain_swapchain_info *info)
{
   return chain->update_swapchain_info(*info);
}

void vulkan_filter_chain_notify_sync_index(
      vulkan_filter_chain_t *chain,
      unsigned index)
{
   chain->notify_sync_index(index);
}

bool vulkan_filter_chain_init(vulkan_filter_chain_t *chain)
{
   return chain->init();
}

void vulkan_filter_chain_set_input_texture(
      vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_texture *texture)
{
   chain->set_input_texture(*texture);
}

void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp)
{
   chain->build_offscreen_passes(cmd, *vp);
}

void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp, const float *mvp)
{
   chain->build_viewport_pass(cmd, *vp, mvp);
}


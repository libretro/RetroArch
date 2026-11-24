/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RPYROWAVE_VULKAN_CODEC_TEST_H
#define RPYROWAVE_VULKAN_CODEC_TEST_H
#include "gpu_packet_vectors.h"

typedef struct rpy_vk_wire
{
   rpy_vk_test_t *base;
   VkDescriptorSetLayout set_layout;
   VkDescriptorPool pool;
   VkDescriptorSet sets[2];
   VkPipelineLayout pipeline_layout;
   VkPipeline pipeline;
   VkShaderModule shader;
   rpy_vk_buffer_t blocks;
   rpy_vk_buffer_t block_upload;
   rpy_vk_buffer_t workspace;
   rpy_vk_buffer_t upload;
   rpy_vk_buffer_t readback;
   rpy_vk_buffer_t uniform;
   rpy_vk_buffer_t backup;
   size_t stride;
   rpyrowave_packet_parameters_t decode;
} rpy_vk_wire_t;

static int rpy_vk_packet_pipeline(rpy_vk_wire_t *test, const char *path)
{
   rpy_vk_test_t *base = test->base;
   VkDescriptorSetLayoutBinding bindings[5];
   VkDescriptorSetLayoutCreateInfo layout;
   VkDescriptorPoolSize sizes[2];
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetLayout layouts[2];
   VkDescriptorSetAllocateInfo allocate;
   VkPipelineLayoutCreateInfo pipeline_layout;
   VkShaderModuleCreateInfo module;
   VkComputePipelineCreateInfo pipeline;
   uint8_t *shader = NULL;
   size_t bytes;
   unsigned i;
   int success = 0;
   shader = rpy_gpu_test_file(path, &bytes);
   if (!shader || (bytes & 3))
      goto end;
   for (i = 0; i < 5; i++)
   {
      bindings[i].binding = i;
      bindings[i].descriptorType = i == 4 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
         : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].pImmutableSamplers = NULL;
   }
   layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layout.pNext = NULL;
   layout.flags = 0;
   layout.bindingCount = 5;
   layout.pBindings = bindings;
   if (base->vkCreateDescriptorSetLayout(base->device, &layout, NULL, &test->set_layout) != VK_SUCCESS)
      goto end;
   sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   sizes[0].descriptorCount = 8;
   sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
   sizes[1].descriptorCount = 2;
   pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.pNext = NULL;
   pool.flags = 0;
   pool.maxSets = 2;
   pool.poolSizeCount = 2;
   pool.pPoolSizes = sizes;
   if (base->vkCreateDescriptorPool(base->device, &pool, NULL, &test->pool) != VK_SUCCESS)
      goto end;
   layouts[0] = layouts[1] = test->set_layout;
   allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocate.pNext = NULL;
   allocate.descriptorPool = test->pool;
   allocate.descriptorSetCount = 2;
   allocate.pSetLayouts = layouts;
   if (base->vkAllocateDescriptorSets(base->device, &allocate, test->sets) != VK_SUCCESS)
      goto end;
   pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipeline_layout.pNext = NULL;
   pipeline_layout.flags = 0;
   pipeline_layout.setLayoutCount = 1;
   pipeline_layout.pSetLayouts = &test->set_layout;
   pipeline_layout.pushConstantRangeCount = 0;
   pipeline_layout.pPushConstantRanges = NULL;
   if (base->vkCreatePipelineLayout(base->device, &pipeline_layout, NULL, &test->pipeline_layout) != VK_SUCCESS)
      goto end;
   module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   module.pNext = NULL;
   module.flags = 0;
   module.codeSize = bytes;
   module.pCode = (const uint32_t*)shader;
   if (base->vkCreateShaderModule(base->device, &module, NULL, &test->shader) != VK_SUCCESS)
      goto end;
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.pNext = NULL;
   pipeline.flags = 0;
   pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.pNext = NULL;
   pipeline.stage.flags = 0;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = test->shader;
   pipeline.stage.pName = "main";
   pipeline.stage.pSpecializationInfo = NULL;
   pipeline.layout = test->pipeline_layout;
   pipeline.basePipelineHandle = VK_NULL_HANDLE;
   pipeline.basePipelineIndex = -1;
   success = base->vkCreateComputePipelines(base->device, VK_NULL_HANDLE, 1,
         &pipeline, NULL, &test->pipeline) == VK_SUCCESS;
end:
   free(shader);
   return success;
}

static void rpy_vk_packet_descriptors(rpy_vk_wire_t *test, const rpy_gpu_vector_t *vector,
      const rpyrowave_gpu_layout_t *layout)
{
   rpy_vk_test_t *base = test->base;
   unsigned set;
   for (set = 0; set < 2; set++)
   {
      VkDescriptorBufferInfo buffers[5];
      VkWriteDescriptorSet writes[5];
      unsigned i;
      buffers[0].buffer = base->atlas[set].buffer;
      buffers[0].range = vector->bytes;
      buffers[1].buffer = test->blocks.buffer;
      buffers[1].range = layout->block_count * sizeof(rpyrowave_gpu_block_t);
      buffers[2].buffer = test->workspace.buffer;
      buffers[2].range = layout->workspace_bytes;
      buffers[3].buffer = base->atlas[set ^ 1].buffer;
      buffers[3].range = vector->bytes;
      buffers[4].buffer = test->uniform.buffer;
      buffers[4].range = sizeof(rpyrowave_packet_parameters_t);
      for (i = 0; i < 5; i++)
      {
         buffers[i].offset = 0;
         writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         writes[i].pNext = NULL;
         writes[i].dstSet = test->sets[set];
         writes[i].dstBinding = i;
         writes[i].dstArrayElement = 0;
         writes[i].descriptorCount = 1;
         writes[i].descriptorType = i == 4 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         writes[i].pImageInfo = NULL;
         writes[i].pBufferInfo = &buffers[i];
         writes[i].pTexelBufferView = NULL;
      }
      base->vkUpdateDescriptorSets(base->device, 5, writes, 0, NULL);
   }
}

static int rpy_vk_packet_parameters(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_vk_wire_t *test = (rpy_vk_wire_t*)userdata;
   (void)x;
   (void)y;
   memcpy((uint8_t*)test->uniform.mapped + parameters->operation * test->stride,
         parameters, sizeof(*parameters));
   test->decode = *parameters;
   return 1;
}

static int rpy_vk_packet_dispatch(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_vk_wire_t *test = (rpy_vk_wire_t*)userdata;
   rpy_vk_test_t *base = test->base;
   unsigned decode = parameters->operation == RPYROWAVE_PACKET_UNPACK;
   uint32_t offset = (uint32_t)(parameters->operation * test->stride);
   VkMemoryBarrier barrier;
   base->vkCmdBindPipeline(base->command, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline);
   base->vkCmdBindDescriptorSets(base->command, VK_PIPELINE_BIND_POINT_COMPUTE,
         test->pipeline_layout, 0, 1, &test->sets[decode], 1, &offset);
   base->vkCmdDispatch(base->command, x, y, 1);
   barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext = NULL;
   barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
   base->vkCmdPipelineBarrier(base->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
   return 1;
}

static int rpy_vk_packet_dwt(rpy_vk_wire_t *test, const rpy_gpu_vector_t *vector, unsigned inverse)
{
   rpy_vk_test_t *base = test->base;
   base->cursor = 0;
   base->vkCmdBindPipeline(base->command, VK_PIPELINE_BIND_POINT_COMPUTE, base->pipeline);
   return !rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
         vector->sequence.chroma_444, inverse, rpy_vk_dispatch, base);
}

static void rpy_vk_packet_copy(rpy_vk_wire_t *test, size_t bytes)
{
   rpy_vk_test_t *base = test->base;
   VkBufferCopy copy;
   copy.srcOffset = copy.dstOffset = 0;
   copy.size = bytes;
   rpy_vk_buffer_barrier(base, test->workspace.buffer, VK_ACCESS_SHADER_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   base->vkCmdCopyBuffer(base->command, test->workspace.buffer, test->readback.buffer, 1, &copy);
   rpy_vk_buffer_barrier(base, test->readback.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
}

static int rpy_vk_packet_image_compare(const rpy_gpu_vector_t *vector, const float *samples,
      const rpyrowave_image_t *expected)
{
   size_t pixels = (size_t)vector->aligned_width * vector->aligned_height;
   unsigned component;
   double maximum = 0.0;
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component && !vector->sequence.chroma_444 ? 1 : 0;
      unsigned x;
      unsigned y;
      for (y = 0; y < vector->sequence.height >> shift; y++)
         for (x = 0; x < vector->sequence.width >> shift; x++)
         {
            double value = samples[component * pixels + y * vector->aligned_width + x] + 0.5;
            double error;
            if (value < 0.0) value = 0.0;
            if (value > 1.0) value = 1.0;
            error = fabs(value - expected->planes[component][y * expected->stride + x]);
            if (!(error <= 0.0001))
               return 0;
            if (error > maximum)
               maximum = error;
         }
   }
   printf("GPU packet decode versus CPU: maximum error %.9f\n", maximum);
   return 1;
}

static int rpy_vk_codec_run(rpy_vk_test_t *base, rpy_gpu_vector_t *vector,
      const char *packet_path, const char *frame_path, const char *prefix)
{
   static const uint32_t budgets[6] = { 8, 512, 4096, 32768, 262144, 262144 };
   rpy_vk_wire_t test;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_gpu_block_t *blocks = NULL;
   rpyrowave_decoder_t *cpu = NULL;
   rpyrowave_image_t expected;
   rpyrowave_sequence_t sequence;
   uint8_t *frame = NULL;
   uint8_t *workspace = NULL;
   size_t frame_bytes;
   size_t block_bytes;
   size_t alignment;
   unsigned budget;
   VkBufferCopy copy;
   int success = 0;
   memset(&test, 0, sizeof(test));
   test.base = base;
   frame = rpy_gpu_test_file(frame_path, &frame_bytes);
   RPY_VK_CHECK(frame && !rpyrowave_gpu_layout(&vector->sequence, 262144, &layout));
   block_bytes = layout.block_count * sizeof(*blocks);
   blocks = (rpyrowave_gpu_block_t*)malloc(block_bytes);
   workspace = (uint8_t*)calloc(1, layout.workspace_bytes);
   cpu = rpyrowave_decoder_new(vector->sequence.width, vector->sequence.height,
         vector->sequence.chroma_444, 16 * 1024 * 1024);
   RPY_VK_CHECK(blocks && workspace && cpu && rpy_vk_packet_pipeline(&test, packet_path));
   RPY_VK_CHECK(!rpyrowave_gpu_blocks(&layout, blocks, layout.block_count));
   memcpy(workspace + layout.wire_offset, frame, frame_bytes);
   RPY_VK_CHECK(!rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes,
            frame_bytes, 0, &test.decode));
   alignment = (size_t)base->properties.limits.minUniformBufferOffsetAlignment;
   RPY_VK_CHECK(alignment > 0 && alignment <= 65536);
   test.stride = (sizeof(rpyrowave_packet_parameters_t) + alignment - 1) / alignment * alignment;
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.blocks, block_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.workspace, layout.workspace_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.backup, vector->bytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.block_upload, block_bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.upload, layout.workspace_bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.readback, layout.workspace_bytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(base, &test.uniform, test.stride * 6,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   memcpy(test.block_upload.mapped, blocks, block_bytes);
   memcpy(test.upload.mapped, workspace, layout.workspace_bytes);
   memcpy((uint8_t*)test.uniform.mapped + RPYROWAVE_PACKET_UNPACK * test.stride,
         &test.decode, sizeof(test.decode));
   rpy_vk_packet_descriptors(&test, vector, &layout);
   RPY_VK_CHECK(base->vkResetFences(base->device, 1, &base->fence) == VK_SUCCESS);
   RPY_VK_CHECK(base->vkResetCommandBuffer(base->command, 0) == VK_SUCCESS && rpy_vk_begin(base));
   copy.srcOffset = copy.dstOffset = 0;
   copy.size = block_bytes;
   base->vkCmdCopyBuffer(base->command, test.block_upload.buffer, test.blocks.buffer, 1, &copy);
   copy.size = layout.workspace_bytes;
   base->vkCmdCopyBuffer(base->command, test.upload.buffer, test.workspace.buffer, 1, &copy);
   rpy_vk_buffer_barrier(base, test.blocks.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   rpy_vk_buffer_barrier(base, test.workspace.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   RPY_VK_CHECK(!rpyrowave_gpu_decode_record(&layout, &test.decode, rpy_vk_packet_dispatch, &test));
   RPY_VK_CHECK(rpy_vk_packet_dwt(&test, vector, 1));
   rpy_vk_buffer_barrier(base, base->atlas[0].buffer, VK_ACCESS_SHADER_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   copy.size = vector->bytes;
   base->vkCmdCopyBuffer(base->command, base->atlas[0].buffer, test.backup.buffer, 1, &copy);
   rpy_vk_buffer_barrier(base, test.backup.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   rpy_vk_packet_copy(&test, layout.workspace_bytes);
   RPY_VK_CHECK(rpy_vk_submit(base, vector->bytes));
   RPY_VK_CHECK(!((uint32_t*)test.readback.mapped)[3]);
   RPY_VK_CHECK(rpy_gpu_vector_compare(vector, (const float*)base->readback.mapped));
   sequence = vector->sequence;
   for (budget = 0; budget < 6; budget++)
   {
      uint32_t *control;
      int fixed = budget == 5 ? 128 : -1;
      sequence.sequence = budget;
      RPY_VK_CHECK(!rpyrowave_gpu_encode_record(&layout, &sequence, budgets[budget], fixed,
               rpy_vk_packet_parameters, &test));
      test.decode.operation = RPYROWAVE_PACKET_UNPACK;
      memcpy((uint8_t*)test.uniform.mapped + RPYROWAVE_PACKET_UNPACK * test.stride,
            &test.decode, sizeof(test.decode));
      RPY_VK_CHECK(base->vkResetFences(base->device, 1, &base->fence) == VK_SUCCESS);
      RPY_VK_CHECK(base->vkResetCommandBuffer(base->command, 0) == VK_SUCCESS && rpy_vk_begin(base));
      rpy_vk_buffer_barrier(base, base->atlas[0].buffer,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
      copy.size = vector->bytes;
      base->vkCmdCopyBuffer(base->command, test.backup.buffer, base->atlas[0].buffer, 1, &copy);
      rpy_vk_buffer_barrier(base, base->atlas[0].buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      RPY_VK_CHECK(rpy_vk_packet_dwt(&test, vector, 0));
      RPY_VK_CHECK(!rpyrowave_gpu_encode_record(&layout, &sequence, budgets[budget], fixed,
               rpy_vk_packet_dispatch, &test));
      RPY_VK_CHECK(!rpyrowave_gpu_decode_record(&layout, &test.decode, rpy_vk_packet_dispatch, &test));
      RPY_VK_CHECK(rpy_vk_packet_dwt(&test, vector, 1));
      rpy_vk_packet_copy(&test, layout.workspace_bytes);
      RPY_VK_CHECK(rpy_vk_submit(base, vector->bytes));
      control = (uint32_t*)test.readback.mapped;
      printf("GPU encoded budget %u: %u bytes, quantizer %u, %u active blocks\n",
            budgets[budget], control[1], control[0], control[2]);
      RPY_VK_CHECK(!control[3] && control[1] >= 8 && control[1] <= budgets[budget]);
      RPY_VK_CHECK(!rpyrowave_decode_frame(cpu, (const uint8_t*)test.readback.mapped + layout.wire_offset,
               control[1], 0, &expected));
      RPY_VK_CHECK(rpy_vk_packet_image_compare(vector, (const float*)base->readback.mapped, &expected));
      if (prefix)
      {
         char *path = (char*)malloc(strlen(prefix) + 40);
         FILE *file;
         int written;
         RPY_VK_CHECK(path != NULL);
         sprintf(path, "%s.%u%s.pyro", prefix, budgets[budget], fixed >= 0 ? "-fixed" : "");
         file = fopen(path, "wb");
         free(path);
         RPY_VK_CHECK(file != NULL);
         written = fwrite((const uint8_t*)test.readback.mapped + layout.wire_offset,
               1, control[1], file) == control[1];
         written = fclose(file) == 0 && written;
         RPY_VK_CHECK(written);
      }
   }
   for (budget = 0; budget < 2; budget++)
   {
      RPY_VK_CHECK(rpy_gpu_fault_prepare(&layout, workspace, frame, frame_bytes, budget, &test.decode));
      memcpy(test.upload.mapped, workspace, layout.workspace_bytes);
      memcpy((uint8_t*)test.uniform.mapped + RPYROWAVE_PACKET_UNPACK * test.stride,
            &test.decode, sizeof(test.decode));
      RPY_VK_CHECK(base->vkResetFences(base->device, 1, &base->fence) == VK_SUCCESS);
      RPY_VK_CHECK(base->vkResetCommandBuffer(base->command, 0) == VK_SUCCESS && rpy_vk_begin(base));
      rpy_vk_buffer_barrier(base, test.workspace.buffer,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
      copy.size = layout.workspace_bytes;
      base->vkCmdCopyBuffer(base->command, test.upload.buffer, test.workspace.buffer, 1, &copy);
      rpy_vk_buffer_barrier(base, test.workspace.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      RPY_VK_CHECK(!rpyrowave_gpu_decode_record(&layout, &test.decode, rpy_vk_packet_dispatch, &test));
      rpy_vk_packet_copy(&test, layout.workspace_bytes);
      RPY_VK_CHECK(rpy_vk_submit(base, vector->bytes));
      RPY_VK_CHECK(((const uint32_t*)test.readback.mapped)[3] != 0);
   }
   puts("GPU magnitude/sign truncation guards: PASS");
   RPY_VK_CHECK(!base->errors);
   success = 1;
end:
   rpy_vk_buffer_free(base, &test.blocks);
   rpy_vk_buffer_free(base, &test.block_upload);
   rpy_vk_buffer_free(base, &test.workspace);
   rpy_vk_buffer_free(base, &test.upload);
   rpy_vk_buffer_free(base, &test.readback);
   rpy_vk_buffer_free(base, &test.uniform);
   rpy_vk_buffer_free(base, &test.backup);
   if (test.pipeline) base->vkDestroyPipeline(base->device, test.pipeline, NULL);
   if (test.shader) base->vkDestroyShaderModule(base->device, test.shader, NULL);
   if (test.pipeline_layout) base->vkDestroyPipelineLayout(base->device, test.pipeline_layout, NULL);
   if (test.pool) base->vkDestroyDescriptorPool(base->device, test.pool, NULL);
   if (test.set_layout) base->vkDestroyDescriptorSetLayout(base->device, test.set_layout, NULL);
   rpyrowave_decoder_free(cpu);
   free(blocks);
   free(workspace);
   free(frame);
   puts(success ? "Vulkan GPU PyroWave packets: PASS" : "Vulkan GPU PyroWave packets: FAIL");
   return success;
}
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include "gpu_vulkan_context.h"
#include "gpu_vulkan_codec.h"

int main(int argc, char **argv)
{
   rpy_vk_test_t *test = (rpy_vk_test_t*)calloc(1, sizeof(*test));
   rpy_gpu_vector_t *vector = NULL;
   uint8_t *shader = NULL;
   size_t shader_size;
   unsigned mode;
   size_t i;
   double maximum = 0.0;
   int success = 0;
   if ((argc != 4 && argc != 6) || !test)
   {
      fprintf(stderr, "Usage: gpu_vulkan dwt.spv frame.pyro reference.u16 [packet.spv output-prefix]\n");
      free(test);
      return EXIT_FAILURE;
   }
   vector = rpy_gpu_vector_load(argv[2], argv[3]);
   shader = rpy_gpu_test_file(argv[1], &shader_size);
   RPY_VK_CHECK(vector && shader && rpy_vk_initialize(test));
   RPY_VK_CHECK(rpy_vk_pipeline(test, shader, shader_size));
   test->parameter_stride = (size_t)test->properties.limits.minUniformBufferOffsetAlignment;
   if (test->parameter_stride < sizeof(rpyrowave_dwt_parameters_t))
      test->parameter_stride = sizeof(rpyrowave_dwt_parameters_t);
   RPY_VK_CHECK(test->parameter_stride <= 65536);
   for (mode = 0; mode < 2; mode++)
      RPY_VK_CHECK(rpy_vk_buffer_create(test, &test->atlas[mode], vector->bytes,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(test, &test->upload, vector->bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(test, &test->readback, vector->bytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   RPY_VK_CHECK(rpy_vk_buffer_create(test, &test->uniform, test->parameter_stride * 100,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   memcpy(test->upload.mapped, vector->atlas, vector->bytes);
   for (mode = 0; mode < 2; mode++)
   {
      test->cursor = 0;
      RPY_VK_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
               vector->sequence.chroma_444, mode, rpy_vk_parameters, test));
   }
   rpy_vk_descriptors(test, vector->bytes);
   RPY_VK_CHECK(rpy_vk_begin(test));
   {
      VkBufferCopy copy;
      copy.srcOffset = copy.dstOffset = 0;
      copy.size = vector->bytes;
      test->vkCmdCopyBuffer(test->command, test->upload.buffer, test->atlas[0].buffer, 1, &copy);
   }
   rpy_vk_buffer_barrier(test, test->atlas[0].buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   test->cursor = 0;
   RPY_VK_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
            vector->sequence.chroma_444, 1, rpy_vk_dispatch, test));
   RPY_VK_CHECK(rpy_vk_submit(test, vector->bytes));
   RPY_VK_CHECK(rpy_gpu_vector_compare(vector, (const float*)test->readback.mapped));
   memcpy(vector->atlas, test->readback.mapped, vector->bytes);
   if (argc == 6)
   {
      RPY_VK_CHECK(rpy_vk_codec_run(test, vector, argv[4], argv[2], argv[5]));
      RPY_VK_CHECK(!test->errors);
      success = 1;
      goto end;
   }
   RPY_VK_CHECK(test->vkResetFences(test->device, 1, &test->fence) == VK_SUCCESS);
   RPY_VK_CHECK(test->vkResetCommandBuffer(test->command, 0) == VK_SUCCESS && rpy_vk_begin(test));
   for (mode = 0; mode < 2; mode++)
   {
      test->cursor = 0;
      RPY_VK_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
               vector->sequence.chroma_444, mode, rpy_vk_dispatch, test));
   }
   RPY_VK_CHECK(rpy_vk_submit(test, vector->bytes));
   for (i = 0; i < vector->bytes / sizeof(float); i++)
   {
      double error = fabs((double)vector->atlas[i] - ((const float*)test->readback.mapped)[i]);
      RPY_VK_CHECK(error <= 0.0001);
      if (error > maximum)
         maximum = error;
   }
   RPY_VK_CHECK(!test->errors);
   printf("GPU forward/inverse round trip: maximum error %.9f\n", maximum);
   success = 1;
end:
   free(shader);
   rpy_gpu_vector_free(vector);
   rpy_vk_free(test);
   puts(success ? "Vulkan compute DWT: PASS" : "Vulkan compute DWT: FAIL");
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

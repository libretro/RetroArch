/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include "gpu_gl_context.h"
#include "gpu_packet_vectors.h"

typedef struct rpy_gl_wire
{
   rpy_gl_test_t gl;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_gpu_block_t *blocks;
   uint8_t *workspace;
   uint8_t *constants;
   size_t constant_stride;
   GLuint program;
   GLuint buffers[4];
} rpy_gl_wire_t;

static int rpy_gl_wire_parameters(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_gl_wire_t *test = (rpy_gl_wire_t*)userdata;
   (void)x;
   (void)y;
   memcpy(test->constants + parameters->operation * test->constant_stride,
         parameters, sizeof(*parameters));
   return 1;
}

static int rpy_gl_wire_dispatch(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_gl_wire_t *test = (rpy_gl_wire_t*)userdata;
   rpy_gl_api_t *api = &test->gl.api;
   unsigned decode = parameters->operation == RPYROWAVE_PACKET_UNPACK;
   api->UseProgram(test->program);
   api->BindBufferBase(RPY_GL_STORAGE, 0, test->gl.buffers[decode ? 1 : 0]);
   api->BindBufferBase(RPY_GL_STORAGE, 1, test->buffers[0]);
   api->BindBufferBase(RPY_GL_STORAGE, 2, test->buffers[1]);
   api->BindBufferBase(RPY_GL_STORAGE, 3, test->gl.buffers[decode ? 0 : 1]);
   api->BindBufferRange(RPY_GL_UNIFORM, 4, test->buffers[2],
         (ptrdiff_t)(parameters->operation * test->constant_stride), sizeof(*parameters));
   api->DispatchCompute(x, y, 1);
   api->MemoryBarrier(RPY_GL_STORAGE_BARRIER);
   return 1;
}

static void rpy_gl_wire_constants(rpy_gl_wire_t *test)
{
   test->gl.api.BindBuffer(RPY_GL_UNIFORM, test->buffers[2]);
   test->gl.api.BufferData(RPY_GL_UNIFORM, (ptrdiff_t)(test->constant_stride * 6),
         test->constants, RPY_GL_DYNAMIC_COPY);
}

static void rpy_gl_wire_control(rpy_gl_wire_t *test, uint32_t control[4])
{
   test->gl.api.MemoryBarrier(0x200);
   test->gl.api.BindBuffer(RPY_GL_STORAGE, test->buffers[1]);
   test->gl.api.GetBufferSubData(RPY_GL_STORAGE, 0, 16, control);
}

static int rpy_gl_wire_image(rpy_gl_wire_t *test, const rpy_gpu_vector_t *vector,
      const rpyrowave_image_t *expected)
{
   size_t pixels = (size_t)vector->aligned_width * vector->aligned_height;
   unsigned component;
   double maximum = 0.0;
   test->gl.api.UseProgram(test->gl.program);
   if (!rpy_gl_execute(&test->gl, vector, 1))
      return 0;
   test->gl.api.MemoryBarrier(0x200);
   test->gl.api.BindBuffer(RPY_GL_STORAGE, test->gl.buffers[0]);
   test->gl.api.GetBufferSubData(RPY_GL_STORAGE, 0, (ptrdiff_t)vector->bytes, vector->readback);
   if (!expected)
      return rpy_gpu_vector_compare(vector, vector->readback);
   for (component = 0; component < 3; component++)
   {
      unsigned x;
      unsigned y;
      unsigned shift = component && !vector->sequence.chroma_444 ? 1 : 0;
      for (y = 0; y < vector->sequence.height >> shift; y++)
         for (x = 0; x < vector->sequence.width >> shift; x++)
         {
            double value = vector->readback[component * pixels + y * vector->aligned_width + x] + 0.5;
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

static int rpy_gl_wire_reject(rpy_gl_wire_t *test, size_t frame_bytes)
{
   rpyrowave_packet_parameters_t decode;
   uint32_t control[4];
   if (rpyrowave_gpu_index_frame(&test->layout, test->workspace,
            test->layout.workspace_bytes, frame_bytes, 0, &decode))
      return 0;
   memcpy(test->constants + RPYROWAVE_PACKET_UNPACK * test->constant_stride, &decode, sizeof(decode));
   test->gl.api.BindBuffer(RPY_GL_STORAGE, test->buffers[1]);
   test->gl.api.BufferData(RPY_GL_STORAGE, test->layout.workspace_bytes,
         test->workspace, RPY_GL_DYNAMIC_COPY);
   rpy_gl_wire_constants(test);
   if (rpyrowave_gpu_decode_record(&test->layout, &decode, rpy_gl_wire_dispatch, test))
      return 0;
   rpy_gl_wire_control(test, control);
   return control[3] != 0 && glGetError() == GL_NO_ERROR;
}

#define RPY_GL_WIRE_CHECK(expr) do { if (!(expr)) { \
   fprintf(stderr, "GL codec failed line %u: %s\n", (unsigned)__LINE__, #expr); goto end; \
} } while (0)

int main(int argc, char **argv)
{
   static const uint32_t budgets[6] = { 8, 512, 4096, 32768, 262144, 262144 };
   rpy_gl_wire_t test;
   rpy_gpu_vector_t *vector = NULL;
   rpyrowave_decoder_t *cpu = NULL;
   rpyrowave_image_t image;
   rpyrowave_packet_parameters_t decode;
   rpyrowave_sequence_t sequence;
   uint8_t *frame = NULL;
   size_t frame_size;
   size_t i;
   unsigned budget;
   GLuint dwt_program;
   GLint alignment;
   uint32_t control[4];
   int loaded;
   int success = 0;
   memset(&test, 0, sizeof(test));
   if (argc != 5 && argc != 6)
   {
      fprintf(stderr, "Usage: gpu_gl_codec dwt.comp packet.comp frame.pyro reference.u16 [output-prefix]\n");
      return EXIT_FAILURE;
   }
   vector = rpy_gpu_vector_load(argv[3], argv[4]);
   RPY_GL_WIRE_CHECK(vector && rpy_gl_setup(&test.gl, vector, argv[1]));
   dwt_program = test.gl.program;
   test.gl.program = 0;
   loaded = rpy_gl_program(&test.gl, argv[2]);
   test.program = test.gl.program;
   test.gl.program = dwt_program;
   RPY_GL_WIRE_CHECK(loaded);
   frame = rpy_gpu_test_file(argv[3], &frame_size);
   RPY_GL_WIRE_CHECK(frame && !rpyrowave_gpu_layout(&vector->sequence, 262144, &test.layout));
   cpu = rpyrowave_decoder_new(vector->sequence.width, vector->sequence.height,
         vector->sequence.chroma_444, 16 * 1024 * 1024);
   test.blocks = (rpyrowave_gpu_block_t*)malloc(test.layout.block_count * sizeof(*test.blocks));
   test.workspace = (uint8_t*)calloc(1, test.layout.workspace_bytes);
   glGetIntegerv(0x8a34, &alignment);
   RPY_GL_WIRE_CHECK(alignment > 0);
   test.constant_stride = (sizeof(rpyrowave_packet_parameters_t) + alignment - 1) / alignment * alignment;
   test.constants = (uint8_t*)calloc(6, test.constant_stride);
   RPY_GL_WIRE_CHECK(cpu && test.blocks && test.workspace && test.constants);
   RPY_GL_WIRE_CHECK(!rpyrowave_gpu_blocks(&test.layout, test.blocks, test.layout.block_count));
   memcpy(test.workspace + test.layout.wire_offset, frame, frame_size);
   RPY_GL_WIRE_CHECK(!rpyrowave_gpu_index_frame(&test.layout, test.workspace,
            test.layout.workspace_bytes, frame_size, 0, &decode));
   memcpy(test.constants + RPYROWAVE_PACKET_UNPACK * test.constant_stride, &decode, sizeof(decode));
   test.gl.api.GenBuffers(4, test.buffers);
   test.gl.api.BindBuffer(RPY_GL_STORAGE, test.buffers[0]);
   test.gl.api.BufferData(RPY_GL_STORAGE, (ptrdiff_t)(test.layout.block_count * sizeof(*test.blocks)),
         test.blocks, RPY_GL_DYNAMIC_COPY);
   test.gl.api.BindBuffer(RPY_GL_STORAGE, test.buffers[1]);
   test.gl.api.BufferData(RPY_GL_STORAGE, test.layout.workspace_bytes, test.workspace, RPY_GL_DYNAMIC_COPY);
   rpy_gl_wire_constants(&test);
   for (i = 0; i < vector->bytes / sizeof(float); i++)
      vector->readback[i] = 123.0f;
   test.gl.api.BindBuffer(RPY_GL_STORAGE, test.gl.buffers[0]);
   test.gl.api.BufferData(RPY_GL_STORAGE, (ptrdiff_t)vector->bytes, vector->readback, RPY_GL_DYNAMIC_COPY);
   RPY_GL_WIRE_CHECK(!rpyrowave_gpu_decode_record(&test.layout, &decode, rpy_gl_wire_dispatch, &test));
   rpy_gl_wire_control(&test, control);
   RPY_GL_WIRE_CHECK(!control[3] && rpy_gl_wire_image(&test, vector, NULL));
   test.gl.api.BindBuffer(RPY_GL_STORAGE, test.buffers[3]);
   test.gl.api.BufferData(RPY_GL_STORAGE, (ptrdiff_t)vector->bytes, NULL, RPY_GL_DYNAMIC_COPY);
   test.gl.api.BindBuffer(0x8f36, test.gl.buffers[0]);
   test.gl.api.BindBuffer(0x8f37, test.buffers[3]);
   test.gl.api.CopyBufferSubData(0x8f36, 0x8f37, 0, 0, (ptrdiff_t)vector->bytes);
   sequence = vector->sequence;
   for (budget = 0; budget < 6; budget++)
   {
      int fixed = budget == 5 ? 128 : -1;
      sequence.sequence = budget;
      test.gl.api.MemoryBarrier(0x200);
      test.gl.api.BindBuffer(0x8f36, test.buffers[3]);
      test.gl.api.BindBuffer(0x8f37, test.gl.buffers[0]);
      test.gl.api.CopyBufferSubData(0x8f36, 0x8f37, 0, 0, (ptrdiff_t)vector->bytes);
      test.gl.api.UseProgram(test.gl.program);
      RPY_GL_WIRE_CHECK(rpy_gl_execute(&test.gl, vector, 0));
      RPY_GL_WIRE_CHECK(!rpyrowave_gpu_encode_record(&test.layout, &sequence, budgets[budget], fixed,
               rpy_gl_wire_parameters, &test));
      rpy_gl_wire_constants(&test);
      RPY_GL_WIRE_CHECK(!rpyrowave_gpu_encode_record(&test.layout, &sequence, budgets[budget], fixed,
               rpy_gl_wire_dispatch, &test));
      rpy_gl_wire_control(&test, control);
      printf("GPU encoded budget %u: %u bytes, quantizer %u, %u active blocks\n",
            budgets[budget], control[1], control[0], control[2]);
      RPY_GL_WIRE_CHECK(!control[3] && control[1] >= 8 && control[1] <= budgets[budget]);
      test.gl.api.GetBufferSubData(RPY_GL_STORAGE, 0, test.layout.workspace_bytes, test.workspace);
      RPY_GL_WIRE_CHECK(!rpyrowave_decode_frame(cpu, test.workspace + test.layout.wire_offset,
               control[1], 0, &image));
      if (argc == 6)
      {
         char *path = (char*)malloc(strlen(argv[5]) + 40);
         FILE *file;
         RPY_GL_WIRE_CHECK(path != NULL);
         sprintf(path, "%s.%u%s.pyro", argv[5], budgets[budget], fixed >= 0 ? "-fixed" : "");
         file = fopen(path, "wb");
         free(path);
         RPY_GL_WIRE_CHECK(file != NULL);
         loaded = fwrite(test.workspace + test.layout.wire_offset, 1, control[1], file) == control[1];
         loaded = fclose(file) == 0 && loaded;
         RPY_GL_WIRE_CHECK(loaded);
      }
      RPY_GL_WIRE_CHECK(!rpyrowave_gpu_index_frame(&test.layout, test.workspace,
               test.layout.workspace_bytes, control[1], 0, &decode));
      memcpy(test.constants + RPYROWAVE_PACKET_UNPACK * test.constant_stride, &decode, sizeof(decode));
      rpy_gl_wire_constants(&test);
      RPY_GL_WIRE_CHECK(!rpyrowave_gpu_decode_record(&test.layout, &decode, rpy_gl_wire_dispatch, &test));
      rpy_gl_wire_control(&test, control);
      RPY_GL_WIRE_CHECK(!control[3] && rpy_gl_wire_image(&test, vector, &image));
   }
   RPY_GL_WIRE_CHECK(rpy_gpu_fault_prepare(&test.layout, test.workspace, frame, frame_size, 0, &decode));
   RPY_GL_WIRE_CHECK(rpy_gl_wire_reject(&test, frame_size));
   RPY_GL_WIRE_CHECK(rpy_gpu_fault_prepare(&test.layout, test.workspace, frame, frame_size, 1, &decode));
   RPY_GL_WIRE_CHECK(rpy_gl_wire_reject(&test, 20));
   puts("GPU magnitude/sign truncation guards: PASS");
   RPY_GL_WIRE_CHECK(glGetError() == GL_NO_ERROR);
   success = 1;
end:
   if (test.gl.api.DeleteBuffers && test.buffers[0])
      test.gl.api.DeleteBuffers(4, test.buffers);
   if (test.gl.api.DeleteProgram && test.program)
      test.gl.api.DeleteProgram(test.program);
   rpy_gl_free(&test.gl);
   rpyrowave_decoder_free(cpu);
   rpy_gpu_vector_free(vector);
   free(test.blocks);
   free(test.workspace);
   free(test.constants);
   free(frame);
   puts(success ? "OpenGL GPU PyroWave packets: PASS" : "OpenGL GPU PyroWave packets: FAIL");
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

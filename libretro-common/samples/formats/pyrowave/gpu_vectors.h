/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */
#ifndef RPYROWAVE_GPU_TEST_VECTORS_H
#define RPYROWAVE_GPU_TEST_VECTORS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <formats/rpyrowave.h>
#include <formats/rpyrowave_gpu.h>

typedef struct rpy_gpu_vector
{
   float *atlas;
   float *readback;
   uint8_t *golden;
   size_t bytes;
   size_t golden_bytes;
   unsigned aligned_width;
   unsigned aligned_height;
   rpyrowave_sequence_t sequence;
} rpy_gpu_vector_t;

static uint8_t *rpy_gpu_test_file(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data;
   long length;
   if (!file)
      return NULL;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0
         || length > 64 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
   {
      fclose(file);
      return NULL;
   }
   data = (uint8_t*)malloc((size_t)length + 1);
   if (!data)
   {
      fclose(file);
      return NULL;
   }
   if (fread(data, 1, (size_t)length, file) != (size_t)length)
   {
      free(data);
      fclose(file);
      return NULL;
   }
   fclose(file);
   data[length] = 0;
   *size = (size_t)length;
   return data;
}

static void rpy_gpu_vector_free(rpy_gpu_vector_t *vector)
{
   if (!vector)
      return;
   free(vector->atlas);
   free(vector->readback);
   free(vector->golden);
   free(vector);
}

static rpy_gpu_vector_t *rpy_gpu_vector_load(const char *path, const char *golden)
{
   rpy_gpu_vector_t *vector = (rpy_gpu_vector_t*)calloc(1, sizeof(*vector));
   uint8_t *stream = NULL;
   float *block = NULL;
   size_t length;
   size_t offset = 8;
   size_t pixels;
   size_t expected;
   if (!vector)
      return NULL;
   stream = rpy_gpu_test_file(path, &length);
   if (!stream || rpyrowave_read_sequence(stream, length, &vector->sequence)
         || vector->sequence.width > 4096 || vector->sequence.height > 4096)
      goto error;
   vector->aligned_width  = (vector->sequence.width + 31) & ~31U;
   vector->aligned_height = (vector->sequence.height + 31) & ~31U;
   if (vector->aligned_width < 128)
      vector->aligned_width = 128;
   if (vector->aligned_height < 128)
      vector->aligned_height = 128;
   pixels = (size_t)vector->aligned_width * vector->aligned_height;
   vector->bytes    = pixels * 3 * sizeof(float);
   vector->atlas    = (float*)calloc(pixels * 3, sizeof(float));
   vector->readback = (float*)malloc(vector->bytes);
   vector->golden   = rpy_gpu_test_file(golden, &vector->golden_bytes);
   block = (float*)malloc(1024 * sizeof(float));
   expected = (size_t)vector->sequence.width * vector->sequence.height;
   expected = vector->sequence.chroma_444 ? expected * 6 : expected * 3;
   if (!vector->atlas || !vector->readback || !vector->golden || !block
         || vector->golden_bytes != expected)
      goto error;
   while (offset < length)
   {
      rpyrowave_block_t header;
      rpyrowave_location_t location;
      float *target;
      uint32_t count;
      unsigned x;
      unsigned y;
      if (rpyrowave_decode_block(stream + offset, length - offset, block, &header))
         goto error;
      count = rpyrowave_block_layout(&vector->sequence, header.index, &location);
      if (header.index >= count)
         goto error;
      offset += header.bytes;
      target = vector->atlas + location.component * pixels;
      if (location.band & 1)
         target += location.width;
      if (location.band & 2)
         target += (size_t)location.height * vector->aligned_width;
      for (y = 0; y < 32 && y + location.y < location.height; y++)
         for (x = 0; x < 32 && x + location.x < location.width; x++)
            target[(location.y + y) * vector->aligned_width + location.x + x]
               = block[y * 32 + x];
   }
   free(block);
   free(stream);
   return vector;
error:
   free(block);
   free(stream);
   rpy_gpu_vector_free(vector);
   return NULL;
}

static int rpy_gpu_vector_compare(const rpy_gpu_vector_t *vector, const float *samples)
{
   size_t pixels = (size_t)vector->aligned_width * vector->aligned_height;
   size_t offset = 0;
   unsigned component;
   double maximum_error = 0.0;
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component && !vector->sequence.chroma_444 ? 1 : 0;
      unsigned x;
      unsigned y;
      for (y = 0; y < vector->sequence.height >> shift; y++)
         for (x = 0; x < vector->sequence.width >> shift; x++)
         {
            double value = samples[component * pixels
               + y * vector->aligned_width + x] + 0.5;
            unsigned code = vector->golden[offset]
               | ((unsigned)vector->golden[offset + 1] << 8);
            double expected = (double)code / 65535.0;
            double error;
            offset += 2;
            if (value < 0.0)
               value = 0.0;
            if (value > 1.0)
               value = 1.0;
            if (!(value >= 0.0 && value <= 1.0))
               return 0;
            error = fabs(value - expected);
            if (error > maximum_error)
               maximum_error = error;
         }
   }
   printf("GPU inverse versus reference: maximum error %.9f\n", maximum_error);
   return maximum_error <= 0.003;
}

#endif

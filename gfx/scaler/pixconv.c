#include "pixconv.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void conv_0rgb1555_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 10) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 3) | (g >> 2);
         b = (b << 3) | (b >> 2);

         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

void conv_0rgb1555_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t b = (col >>  0) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t r = (col >> 10) & 0x1f;
         b = (b << 3) | (b >> 2);
         g = (g << 3) | (g >> 2);
         r = (r << 3) | (r >> 2);

         *out++ = b;
         *out++ = g;
         *out++ = r;
      }
   }
}

void conv_bgr24_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint8_t *input = (const uint8_t*)input_;
   uint32_t *output     = (uint32_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 2, input += in_stride)
   {
      const uint8_t *inp = input;
      for (int w = 0; w < width; w++)
      {
         uint32_t b = *inp++;
         uint32_t g = *inp++;
         uint32_t r = *inp++;
         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

void conv_argb8888_0rgb1555(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output      = (uint16_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 2)
   {
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint16_t r = (col >> 19) & 0x1f;
         uint16_t g = (col >> 11) & 0x1f;
         uint16_t b = (col >>  3) & 0x1f;
         output[w] = (r << 10) | (g << 5) | (b << 0);
      }
   }
}

void conv_argb8888_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride >> 2)
   {
      uint8_t *out = output;
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         *out++ = (uint8_t)(col >>  0);
         *out++ = (uint8_t)(col >>  8);
         *out++ = (uint8_t)(col >> 16);
      }
   }
}

void conv_copy(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int copy_len = abs(out_stride);
   if (abs(in_stride) < copy_len)
      copy_len = abs(in_stride);

   const uint8_t *input = (const uint8_t*)input_;
   uint8_t *output      = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride)
      memcpy(output, input, copy_len);
}


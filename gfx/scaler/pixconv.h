#ifndef PIXCONV_H__
#define PIXCONV_H__

void conv_0rgb1555_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_bgr24_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_0rgb1555(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_0rgb1555_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_copy(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

#endif


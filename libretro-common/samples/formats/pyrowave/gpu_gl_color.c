/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include "gpu_gl_context.h"
#include <float.h>

typedef void (APIENTRY *rpy_gl_bind_image_t)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);

static double rpy_color_clip(double value)
{
   return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

#include "gpu_color_math.h"

static double rpy_color_uncertainty;
static unsigned rpy_color_hdr_samples, rpy_color_negative_samples;

static double rpy_color_rgb(const float *rgb, const rpyrowave_color_parameters_t *p,
      int x, int y, unsigned channel, unsigned stride)
{
   const float *pixel;
   double value;
   unsigned col;
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if ((unsigned)x >= p->width) x = (int)p->width - 1;
   if ((unsigned)y >= p->height) y = (int)p->height - 1;
   if (p->flip_y) y = (int)p->height - 1 - y;
   pixel = rgb + ((p->texture_y + y) * stride + p->texture_x + x) * 4;
   if (p->texture_mode == RPYROWAVE_COLOR_CODE) return rpy_color_clip(pixel[channel]);
   value = pixel[channel];
   if (p->colorimetry & 1)
   {
      double magnitude = 0.0;
      double uncertainty;
      double encoded;
      double upper;
      value = 0.0;
      for (col = 0; col < 3; col++)
      {
         double term = rpy_color_to2020[channel * 3 + col] * pixel[col];
         value += term;
         magnitude += fabs(term);
      }
      /* Bound FP32 dot-product cancellation before the nonlinear transfer. */
      uncertainty = magnitude * (8.0 * FLT_EPSILON);
      encoded = rpy_color_transfer(value, p->colorimetry & 2, 0);
      upper = rpy_color_transfer(value + uncertainty, p->colorimetry & 2, 0) - encoded;
      uncertainty = encoded - rpy_color_transfer(value - uncertainty, p->colorimetry & 2, 0);
      if (upper > uncertainty) uncertainty = upper;
      if (uncertainty > rpy_color_uncertainty) rpy_color_uncertainty = uncertainty;
   }
   return rpy_color_transfer(value, p->colorimetry & 2, 0);
}

static double rpy_color_expected(const float *rgb, const rpyrowave_color_parameters_t *p,
      unsigned x, unsigned y, unsigned component, unsigned stride)
{
   double channels[3];
   double red = p->red;
   double blue = p->blue;
   double luma;
   unsigned channel;
   unsigned shift = component && !p->chroma_444 ? 1 : 0;
   unsigned cols = p->width >> shift;
   unsigned rows = p->height >> shift;
   rpy_color_uncertainty = 0.0;
   if (x >= cols) x = cols - 1;
   if (y >= rows) y = rows - 1;
   for (channel = 0; channel < 3; channel++)
   {
      if (shift)
      {
         int cx = (int)x * 2;
         int cy = (int)y * 2;
         double center = rpy_color_rgb(rgb, p, cx, cy, channel, stride)
            + rpy_color_rgb(rgb, p, cx, cy + 1, channel, stride);
         double right = rpy_color_rgb(rgb, p, cx + 1, cy, channel, stride)
            + rpy_color_rgb(rgb, p, cx + 1, cy + 1, channel, stride);
         double left = rpy_color_rgb(rgb, p, cx - 1, cy, channel, stride)
            + rpy_color_rgb(rgb, p, cx - 1, cy + 1, channel, stride);
         channels[channel] = p->chroma_left
            ? center / 4.0 + (left + right) / 8.0 : (center + right) / 4.0;
      }
      else channels[channel] = rpy_color_rgb(rgb, p, (int)x, (int)y, channel, stride);
   }
   luma = red * channels[0] + (1.0 - red - blue) * channels[1] + blue * channels[2];
   if (component == 1) return rpy_color_clip(p->chroma_center
         + (channels[2] - luma) * p->chroma_scale / (2.0 * (1.0 - blue))) - 0.5;
   if (component == 2) return rpy_color_clip(p->chroma_center
         + (channels[0] - luma) * p->chroma_scale / (2.0 * (1.0 - red))) - 0.5;
   return rpy_color_clip(p->luma_offset + luma * p->luma_scale) - 0.5;
}

static double rpy_color_atlas(const float *atlas, const rpyrowave_color_parameters_t *p,
      int x, int y, unsigned component)
{
   unsigned shift = component && !p->chroma_444 ? 1 : 0;
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if ((unsigned)x >= p->width >> shift) x = (int)(p->width >> shift) - 1;
   if ((unsigned)y >= p->height >> shift) y = (int)(p->height >> shift) - 1;
   return rpy_color_clip(atlas[component * p->image_width * p->image_height
         + y * p->image_width + x] + 0.5);
}

static double rpy_color_chroma(const float *atlas, const rpyrowave_color_parameters_t *p,
      unsigned x, unsigned y, unsigned component)
{
   double cx = x * 0.5 - (p->chroma_left ? 0.0 : 0.25);
   double cy = y * 0.5 - 0.25;
   int ix = (int)floor(cx);
   int iy = (int)floor(cy);
   double wx = cx - ix;
   double wy = cy - iy;
   if (p->chroma_444) return rpy_color_atlas(atlas, p, (int)x, (int)y, component);
   return (1.0 - wy) * ((1.0 - wx) * rpy_color_atlas(atlas, p, ix, iy, component)
         + wx * rpy_color_atlas(atlas, p, ix + 1, iy, component))
      + wy * ((1.0 - wx) * rpy_color_atlas(atlas, p, ix, iy + 1, component)
         + wx * rpy_color_atlas(atlas, p, ix + 1, iy + 1, component));
}

static int rpy_color_run(rpy_gl_test_t *gl, rpy_gl_bind_image_t bind_image,
      unsigned width, unsigned height, unsigned chroma_444, unsigned texture_mode, unsigned *cases)
{
   static const unsigned depths[4] = { 8, 10, 12, 16 };
   rpyrowave_sequence_t sequence;
   rpyrowave_color_parameters_t p;
   unsigned texture_width = width + 4;
   unsigned texture_height = height + 6;
   size_t texture_bytes = (size_t)texture_width * texture_height * 4 * sizeof(float);
   float *rgb = (float*)malloc(texture_bytes);
   float *output = (float*)malloc(texture_bytes);
   float *atlas = NULL;
   GLuint textures[2] = { 0, 0 };
   size_t atlas_bytes;
   size_t i;
   unsigned configuration;
   int success = 0;
   memset(&sequence, 0, sizeof(sequence));
   sequence.width = width;
   sequence.height = height;
   sequence.chroma_444 = chroma_444;
   if (!rgb || !output || rpyrowave_gpu_color_parameters(&sequence, 8, 0, 0,
            texture_width, texture_height, 2, 3, &p)) goto end;
   atlas_bytes = (size_t)p.image_width * p.image_height * 3 * sizeof(float);
   atlas = (float*)malloc(atlas_bytes);
   if (!atlas) goto end;
   for (i = 0; i < texture_bytes / sizeof(float); i++)
      rgb[i] = i % 4 == 3 ? 0.125f : (texture_mode ? (float)rpy_color_sample((unsigned)(i / 4), (unsigned)(i % 4))
            : (float)((i * 13 + i / 73) % 257) / 256.0f);
   glGenTextures(2, textures);
   glBindTexture(GL_TEXTURE_2D, textures[0]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, 0x8814, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, rgb);
   glBindTexture(GL_TEXTURE_2D, textures[1]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, 0x8814, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
   bind_image(3, textures[1], 0, GL_FALSE, 0, 0x88b9, 0x8814);
   gl->api.BindBuffer(RPY_GL_STORAGE, gl->buffers[0]);
   gl->api.BufferData(RPY_GL_STORAGE, (ptrdiff_t)atlas_bytes, NULL, RPY_GL_DYNAMIC_COPY);
   gl->api.BindBufferBase(RPY_GL_STORAGE, 1, gl->buffers[0]);
   gl->api.BindBufferBase(RPY_GL_STORAGE, 2, gl->buffers[0]);
   gl->api.BindBufferBase(RPY_GL_UNIFORM, 4, gl->buffers[2]);
   for (configuration = 0; configuration < (texture_mode ? 256U : 64U); configuration++)
   {
      unsigned depth = depths[configuration & 3];
      unsigned x;
      unsigned y;
      unsigned component;
      sequence.transform_bt2020 = (configuration >> 2) & 1;
      sequence.limited_range    = (configuration >> 3) & 1;
      sequence.chroma_left      = (configuration >> 4) & 1;
      sequence.primaries_bt2020  = (configuration >> 6) & 1;
      sequence.transfer_pq      = (configuration >> 7) & 1;
      if (rpyrowave_gpu_color_parameters(&sequence, depth, 0, (configuration >> 5) & 1,
               texture_width, texture_height, 2, 3, &p)) goto end;
      p.texture_mode = texture_mode ? RPYROWAVE_COLOR_SCRGB : RPYROWAVE_COLOR_CODE;
      glBindTexture(GL_TEXTURE_2D, textures[0]);
      gl->api.BindBuffer(RPY_GL_UNIFORM, gl->buffers[2]);
      gl->api.BufferData(RPY_GL_UNIFORM, sizeof(p), &p, RPY_GL_DYNAMIC_COPY);
      gl->api.DispatchCompute((p.image_width + 7) / 8, (p.image_height + 7) / 8, 3);
      gl->api.MemoryBarrier(RPY_GL_STORAGE_BARRIER | 0x200);
      gl->api.BindBuffer(RPY_GL_STORAGE, gl->buffers[0]);
      gl->api.GetBufferSubData(RPY_GL_STORAGE, 0, (ptrdiff_t)atlas_bytes, atlas);
      for (component = 0; component < 3; component++)
      {
         unsigned shift = component && !chroma_444 ? 1 : 0;
         for (y = 0; y < p.image_height >> shift; y++)
            for (x = 0; x < p.image_width >> shift; x++)
            {
               double expected = rpy_color_expected(rgb, &p, x, y, component, texture_width);
               double actual = atlas[component * p.image_width * p.image_height + y * p.image_width + x];
               if (!(fabs(actual - expected) <= (texture_mode ? 0.00003 + 1.5 * rpy_color_uncertainty : 0.000002)))
               {
                  fprintf(stderr, "Forward color failed config %u at %u,%u,%u: %.9f vs %.9f\n",
                        configuration, x, y, component, actual, expected);
                  goto end;
               }
            }
      }
      p.inverse = 1;
      p.texture_mode = texture_mode;
      for (i = 0; i < texture_bytes / sizeof(float); i++) output[i] = -7.0f;
      glBindTexture(GL_TEXTURE_2D, textures[1]);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_FLOAT, output);
      glBindTexture(GL_TEXTURE_2D, textures[0]);
      gl->api.BindBuffer(RPY_GL_UNIFORM, gl->buffers[2]);
      gl->api.BufferData(RPY_GL_UNIFORM, sizeof(p), &p, RPY_GL_DYNAMIC_COPY);
      gl->api.DispatchCompute((width + 7) / 8, (height + 7) / 8, 1);
      gl->api.MemoryBarrier(0x20 | 0x100);
      glBindTexture(GL_TEXTURE_2D, textures[1]);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output);
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
         {
            double luma = (rpy_color_atlas(atlas, &p, (int)x, (int)y, 0) - p.luma_offset) / p.luma_scale;
            double cb = (rpy_color_chroma(atlas, &p, x, y, 1) - p.chroma_center) / p.chroma_scale;
            double cr = (rpy_color_chroma(atlas, &p, x, y, 2) - p.chroma_center) / p.chroma_scale;
            double expected[4];
            unsigned row = p.flip_y ? height - 1 - y : y;
            expected[0] = luma + cr * (2.0 * (1.0 - p.red));
            expected[2] = luma + cb * (2.0 * (1.0 - p.blue));
            expected[1] = (luma - p.red * expected[0] - p.blue * expected[2]) / (1.0 - p.red - p.blue);
            if (texture_mode) rpy_color_decode_light(expected, &p);
            else
               for (component = 0; component < 3; component++) expected[component] = rpy_color_clip(expected[component]);
            if (texture_mode == RPYROWAVE_COLOR_SDR709)
            {
               double peak = 1.0;
               double peak_scale = 0.0;
               for (component = 0; component < 3; component++)
               {
                  if (expected[component] > peak) peak = expected[component];
                  if (rpy_color_inverse_scale[component] > peak_scale)
                     peak_scale = rpy_color_inverse_scale[component];
               }
               for (component = 0; component < 3; component++)
               {
                  double normalized = rpy_color_clip(expected[component] / peak);
                  double scale = (rpy_color_inverse_scale[component] + normalized * peak_scale) / peak;
                  double lower = normalized - scale * ((p.colorimetry & 2) ? 0.00015 : 0.000001);
                  double slope = lower <= 0.0031308 ? 12.92 : (1.055 / 2.4) * pow(lower, 1.0 / 2.4 - 1.0);
                  expected[component] = rpy_color_transfer(normalized, 0, 0);
                  rpy_color_inverse_scale[component] = scale * slope;
               }
            }
            expected[3] = 1.0;
            for (component = 0; component < 4; component++)
            {
               double actual = output[((3 + row) * texture_width + 2 + x) * 4 + component];
               double tolerance = 0.000003;
               if (texture_mode && component < 3)
               {
                  if (expected[component] > 1.0001) rpy_color_hdr_samples++;
                  if (expected[component] < -0.0001) rpy_color_negative_samples++;
               }
               if (texture_mode && component < 3)
                  tolerance += rpy_color_inverse_scale[component] * ((p.colorimetry & 2) ? 0.00015 : 0.000001);
               if (!(fabs(actual - expected[component]) <= tolerance))
               {
                  fprintf(stderr, "Inverse color failed mode %u config %u at %u,%u,%u: %.9f vs %.9f\n",
                        texture_mode, configuration, x, y, component, actual, expected[component]);
                  goto end;
               }
            }
         }
      for (y = 0; y < texture_height; y++)
         for (x = 0; x < texture_width; x++)
         {
            if (x >= 2 && x < 2 + width && y >= 3 && y < 3 + height) continue;
            for (component = 0; component < 4; component++)
               if (output[(y * texture_width + x) * 4 + component] != -7.0f)
               {
                  fprintf(stderr, "Color conversion overwrote crop guard\n");
                  goto end;
               }
         }
      if (glGetError() != GL_NO_ERROR) goto end;
      (*cases)++;
   }
   success = 1;
end:
   if (textures[0]) glDeleteTextures(2, textures);
   free(rgb);
   free(output);
   free(atlas);
   return success;
}

int main(int argc, char **argv)
{
   rpy_gl_test_t gl;
   rpy_gl_bind_image_t bind_image = NULL;
   unsigned cases = 0;
   int success = 0;
   (void)rpy_gl_setup;
   (void)rpy_gl_execute;
   (void)rpy_gpu_vector_load;
   (void)rpy_gpu_vector_compare;
   memset(&gl, 0, sizeof(gl));
   rpy_color_math_init();
   if (argc != 2 || !rpy_gl_context(&gl) || !rpy_gl_functions(&gl) || !rpy_gl_program(&gl, argv[1]))
      goto end;
#if defined(_WIN32)
   {
      PROC entry = wglGetProcAddress("glBindImageTexture");
      memcpy(&bind_image, &entry, sizeof(entry));
   }
#else
   {
      __eglMustCastToProperFunctionPointerType entry = eglGetProcAddress("glBindImageTexture");
      memcpy(&bind_image, &entry, sizeof(entry));
   }
#endif
   if (!bind_image) goto end;
   gl.api.GenBuffers(3, gl.buffers);
   if (!rpy_color_run(&gl, bind_image, 1, 1, 1, 0, &cases)
         || !rpy_color_run(&gl, bind_image, 2, 2, 0, 0, &cases)
         || !rpy_color_run(&gl, bind_image, 130, 98, 0, 0, &cases)
         || !rpy_color_run(&gl, bind_image, 321, 181, 1, 0, &cases)
         || !rpy_color_run(&gl, bind_image, 1, 1, 1, 1, &cases)
         || !rpy_color_run(&gl, bind_image, 2, 2, 0, 1, &cases)
         || !rpy_color_run(&gl, bind_image, 130, 98, 0, 1, &cases)
         || !rpy_color_run(&gl, bind_image, 1, 1, 1, 2, &cases)
         || !rpy_color_run(&gl, bind_image, 130, 98, 0, 2, &cases)) goto end;
   if (!rpy_color_hdr_samples || !rpy_color_negative_samples) goto end;
   printf("GPU RGB/YCbCr color conversion: %u configurations, %u HDR and %u negative scRGB samples PASS\n",
         cases, rpy_color_hdr_samples, rpy_color_negative_samples);
   success = 1;
end:
   rpy_gl_free(&gl);
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

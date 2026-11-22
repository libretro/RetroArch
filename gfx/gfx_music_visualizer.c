/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <retro_spsc.h>
#include "../libretro-common/audio/dsp_filters/fft/fft.h"
#include "gfx_music_visualizer.h"

#define MUSIC_FFT_LOG2 11
#define MUSIC_FFT_SIZE (1u << MUSIC_FFT_LOG2)
#define MUSIC_BANDS 64
#define MUSIC_HISTORY 256
#define MUSIC_ROWS 64
#define MUSIC_COLUMNS (MUSIC_BANDS * 2)
#define MUSIC_ROW_BYTES 2048
#define MUSIC_QUEUE_BYTES (MUSIC_ROW_BYTES * MUSIC_HISTORY * 2)
#define MUSIC_PI 3.14159265358979323846

struct gfx_music_vertex
{
   float x, y, z, r, g, b;
};

struct gfx_music_row
{
   float height[MUSIC_BANDS + 1][4];
   size_t sequence;
};

struct gfx_music_history
{
   const uint8_t *first, *second;
   size_t first_bytes, bytes;
};

struct gfx_music_visualizer
{
   retro_spsc_t rows;
   fft_t *fft;
   float *depth;
   float pcm[MUSIC_FFT_SIZE * 2];
   float window[MUSIC_FFT_SIZE];
   fft_complex_t output[MUSIC_FFT_SIZE];
   float silence[MUSIC_BANDS + 1][4];
   float sine[MUSIC_COLUMNS + 1], cosine[MUSIC_COLUMNS + 1];
   float palette[161];
   struct gfx_music_vertex vertices[MUSIC_ROWS + 1][MUSIC_COLUMNS + 1];
   size_t sequence, shown_sequence;
   unsigned position, phase;
   bool shown;
};

static double gfx_music_bessel(double value)
{
   double sum = 1.0, term = 1.0;
   unsigned i;
   for (i = 1; i < 32; i++)
   {
      term *= value * value / (4.0 * i * i);
      sum += term;
   }
   return sum;
}

void gfx_music_visualizer_reset(gfx_music_visualizer_t *v)
{
   unsigned band, channel;
   if (!v) return;
   retro_spsc_clear(&v->rows);
   memset(v->pcm, 0, sizeof(v->pcm));
   for (band = 0; band <= MUSIC_BANDS; band++)
      for (channel = 0; channel < 4; channel++)
         v->silence[band][channel] = -40.0f;
   v->sequence = v->shown_sequence = 0;
   v->position = v->phase = 0;
   v->shown = false;
}

void gfx_music_visualizer_free(gfx_music_visualizer_t *v)
{
   if (!v) return;
   fft_free(v->fft);
   retro_spsc_free(&v->rows);
   free(v->depth);
   free(v);
}

void gfx_music_visualizer_invalidate(gfx_music_visualizer_t *v)
{
   if (v) v->shown = false;
}

gfx_music_visualizer_t *gfx_music_visualizer_new(void)
{
   gfx_music_visualizer_t *v = (gfx_music_visualizer_t*)calloc(1, sizeof(*v));
   unsigned i;
   double normal = gfx_music_bessel(12.0);
   if (!v) return NULL;
   v->fft = fft_new(MUSIC_FFT_LOG2);

   v->depth = (float*)malloc(GFX_MUSIC_VISUALIZER_WIDTH
         * GFX_MUSIC_VISUALIZER_HEIGHT * sizeof(*v->depth));
   if (!v->fft || !v->depth || !retro_spsc_init(&v->rows, MUSIC_QUEUE_BYTES))
   {
      gfx_music_visualizer_free(v);
      return NULL;
   }
   for (i = 0; i < MUSIC_FFT_SIZE; i++)
   {
      double phase = ((double)i - MUSIC_FFT_SIZE / 2) / (MUSIC_FFT_SIZE / 2);
      v->window[i] = (float)(gfx_music_bessel(12.0 * sqrt(1.0 - phase * phase)) / normal);
   }
   for (i = 0; i <= MUSIC_COLUMNS; i++)
   {
      double angle = ((double)i / MUSIC_BANDS - 1.0) * MUSIC_PI;
      v->sine[i] = (float)sin(angle);
      v->cosine[i] = (float)cos(angle);
   }
   for (i = 0; i <= 160; i++)
      v->palette[i] = (float)(1.0 / (1.0 + exp(-0.08 * (i * 0.5 - 40.0))));
   gfx_music_visualizer_reset(v);
   return v;
}

static float gfx_music_height(float real, float imag)
{
   float height = (float)(9.0 * log(real * real + imag * imag + 0.0001) - 22.0);
   return height < -40.0f ? -40.0f : height > 40.0f ? 40.0f : height;
}

static void gfx_music_analyse(gfx_music_visualizer_t *v)
{
   unsigned i, channel, band;
   void *memory = NULL;
   struct gfx_music_row *row;
   if (retro_spsc_write_begin(&v->rows, &memory) < MUSIC_ROW_BYTES) return;
   row = (struct gfx_music_row*)memory;
   /* One complex transform carries both real stereo channels. */
   fft_process_forward_stereo_window(v->fft, v->output,
         v->pcm, v->window, v->position);
   for (band = 0; band <= MUSIC_BANDS; band++)
   {
      float *height = row->height[band];
      unsigned first = band * 256 / MUSIC_BANDS;
      unsigned last = first + 256 / MUSIC_BANDS;
      for (channel = 0; channel < 4; channel++) height[channel] = -40.0f;
      if (last > 257) last = 257;
      for (i = first; i < last; i++)
      {
         fft_complex_t a = v->output[i];
         fft_complex_t b = v->output[(MUSIC_FFT_SIZE - i) & (MUSIC_FFT_SIZE - 1)];
         float lr = (a.real + b.real) * 0.5f, li = (a.imag - b.imag) * 0.5f;
         float rr = (a.imag + b.imag) * 0.5f, ri = (b.real - a.real) * 0.5f;
         float values[4];
         values[0] = gfx_music_height(lr, li);
         values[1] = gfx_music_height((lr + rr) * 0.5f, (li + ri) * 0.5f);
         values[2] = gfx_music_height(rr, ri);
         values[3] = gfx_music_height((lr - rr) * 0.5f, (li - ri) * 0.5f);
         for (channel = 0; channel < 4; channel++)
            if (values[channel] > height[channel]) height[channel] = values[channel];
      }
   }
   row->sequence = ++v->sequence;
   retro_spsc_write_end(&v->rows, MUSIC_ROW_BYTES);
}

static float gfx_music_colour(const gfx_music_visualizer_t *v, float height)
{
   int index = (int)((height + 40.0f) * 2.0f + 0.5f);
   return v->palette[index < 0 ? 0 : index > 160 ? 160 : index];
}

static const struct gfx_music_row *gfx_music_row_at(
      const struct gfx_music_history *history, size_t offset)
{
   return (const struct gfx_music_row*)(offset < history->first_bytes
         ? history->first + offset
         : history->second + offset - history->first_bytes);
}

static void gfx_music_vertices(gfx_music_visualizer_t *v,
      const struct gfx_music_history *history)
{
   unsigned row, column;
   for (row = 0; row <= MUSIC_ROWS; row++)
   {
      unsigned age = row ? 1 + (row - 1) * (MUSIC_HISTORY - 2) / (MUSIC_ROWS - 1) : 0;
      const struct gfx_music_row *source = age && age <= history->bytes / MUSIC_ROW_BYTES
         ? gfx_music_row_at(history, history->bytes - age * MUSIC_ROW_BYTES) : NULL;
      float world_z = age * 2.0f;
      float scale = (GFX_MUSIC_VISUALIZER_HEIGHT * 0.5f) / (60.0f + world_z);
      float fog = world_z / 400.0f;
      if (fog > 1.0f) fog = 1.0f;
      if (!age) fog = 1.0f;
      for (column = 0; column <= MUSIC_COLUMNS; column++)
      {
         unsigned band = column < MUSIC_BANDS ? MUSIC_BANDS - column : column - MUSIC_BANDS;
         const float *h = source ? source->height[band] : v->silence[band];
         float weight = band * (1.0f / MUSIC_BANDS);
         float height = h[column < MUSIC_BANDS ? 2 : 0] * (1.0f - weight) + h[1] * weight;
         float radius = 80.0f - height;
         struct gfx_music_vertex *p = &v->vertices[row][column];
         p->x = GFX_MUSIC_VISUALIZER_WIDTH * 0.5f + radius * v->sine[column] * scale;
         p->y = GFX_MUSIC_VISUALIZER_HEIGHT * 0.5f + radius * v->cosine[column] * scale;
         p->z = 1.0f / (60.0f + world_z);
         p->r = 255.0f * (gfx_music_colour(v, height) * (1.0f - fog) + 0.1f * fog);
         p->g = 255.0f * (0.7f * gfx_music_colour(v, h[1]) * (1.0f - fog) + 0.15f * fog);
         p->b = 255.0f * (0.7f * gfx_music_colour(v, h[3]) * (1.0f - fog) + 0.1f * fog);
      }
   }
}

static float gfx_music_edge(const struct gfx_music_vertex *a,
      const struct gfx_music_vertex *b, float x, float y)
{
   return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
}

static void gfx_music_triangle(gfx_music_visualizer_t *v, uint32_t *pixels,
      size_t pitch, bool rgba, const struct gfx_music_vertex *a,
      const struct gfx_music_vertex *b, const struct gfx_music_vertex *c)
{
   float area = gfx_music_edge(a, b, c->x, c->y);
   float min_x = a->x, max_x = a->x, min_y = a->y, max_y = a->y;
   int x0, x1, y0, y1, x, y;
   if (fabs(area) < 0.0001f) return;
   if (b->x < min_x) min_x = b->x;
   if (c->x < min_x) min_x = c->x;
   if (b->x > max_x) max_x = b->x;
   if (c->x > max_x) max_x = c->x;
   if (b->y < min_y) min_y = b->y;
   if (c->y < min_y) min_y = c->y;
   if (b->y > max_y) max_y = b->y;
   if (c->y > max_y) max_y = c->y;
   x0 = min_x < 0 ? 0 : (int)min_x;
   y0 = min_y < 0 ? 0 : (int)min_y;
   x1 = max_x >= GFX_MUSIC_VISUALIZER_WIDTH ? GFX_MUSIC_VISUALIZER_WIDTH - 1 : (int)max_x;
   y1 = max_y >= GFX_MUSIC_VISUALIZER_HEIGHT ? GFX_MUSIC_VISUALIZER_HEIGHT - 1 : (int)max_y;
   area = 1.0f / area;
   for (y = y0; y <= y1; y++)
   {
      uint32_t *out = (uint32_t*)((uint8_t*)pixels + (size_t)y * pitch);
      float *depth = v->depth + y * GFX_MUSIC_VISUALIZER_WIDTH;
      float u = gfx_music_edge(b, c, x0 + 0.5f, y + 0.5f) * area;
      float w = gfx_music_edge(c, a, x0 + 0.5f, y + 0.5f) * area;
      float du = (b->y - c->y) * area, dw = (c->y - a->y) * area;
      for (x = x0; x <= x1; x++, u += du, w += dw)
      {
         float t = 1.0f - u - w;
         if (u >= -0.00001f && w >= -0.00001f && t >= -0.00001f)
         {
            float z = u * a->z + w * b->z + t * c->z;
            if (z >= depth[x])
            {
               uint32_t red = (uint32_t)(u * a->r + w * b->r + t * c->r);
               uint32_t green = (uint32_t)(u * a->g + w * b->g + t * c->g);
               uint32_t blue = (uint32_t)(u * a->b + w * b->b + t * c->b);
               out[x] = rgba ? 0xff000000u | (blue << 16) | (green << 8) | red
                  : 0xff000000u | (red << 16) | (green << 8) | blue;
               depth[x] = z;
            }
         }
      }
   }
}

static void gfx_music_render(gfx_music_visualizer_t *v, uint32_t *pixels,
      size_t pitch, bool rgba, const struct gfx_music_history *history)
{
   unsigned row, column;
   memset(v->depth, 0, GFX_MUSIC_VISUALIZER_WIDTH * GFX_MUSIC_VISUALIZER_HEIGHT * sizeof(float));
   for (row = 0; row < GFX_MUSIC_VISUALIZER_HEIGHT; row++)
   {
      uint32_t *out = (uint32_t*)((uint8_t*)pixels + (size_t)row * pitch);
      for (column = 0; column < GFX_MUSIC_VISUALIZER_WIDTH; column++) out[column] = 0xff192619u;
   }
   gfx_music_vertices(v, history);
   for (row = MUSIC_ROWS; row > 0; row--)
      for (column = 0; column < MUSIC_COLUMNS; column++)
      {
         const struct gfx_music_vertex *a = &v->vertices[row][column];
         const struct gfx_music_vertex *b = &v->vertices[row][column + 1];
         const struct gfx_music_vertex *c = &v->vertices[row - 1][column];
         const struct gfx_music_vertex *d = &v->vertices[row - 1][column + 1];
         gfx_music_triangle(v, pixels, pitch, rgba, a, b, c);
         gfx_music_triangle(v, pixels, pitch, rgba, c, b, d);
      }
}

void gfx_music_visualizer_feed(gfx_music_visualizer_t *v,
      const void *samples, size_t frames, bool floating, unsigned rate)
{
   size_t frame;
   unsigned channel;
   if (!v || !samples || rate < 60) return;
   for (frame = 0; frame < frames; frame++)
   {
      for (channel = 0; channel < 2; channel++)
      {
         float value = floating ? ((const float*)samples)[frame * 2 + channel]
            : ((const int16_t*)samples)[frame * 2 + channel] * (1.0f / 32768.0f);
         if (!(value >= -1.0f && value <= 1.0f))
            value = value > 1.0f ? 1.0f : value < -1.0f ? -1.0f : 0.0f;
         v->pcm[v->position * 2 + channel] = value;
      }
      v->position = (v->position + 1) & (MUSIC_FFT_SIZE - 1);
      v->phase += 60;
      if (v->phase >= rate)
      {
         v->phase -= rate;
         gfx_music_analyse(v);
      }
   }
}

bool gfx_music_visualizer_render(gfx_music_visualizer_t *v,
      uint32_t *pixels, size_t pitch, bool rgba)
{
   const void *first, *second;
   struct gfx_music_history history;
   size_t sequence = 0;
   if (!v || !pixels || pitch < GFX_MUSIC_VISUALIZER_WIDTH * sizeof(*pixels)
         || pitch > INT_MAX || pitch > ((size_t)-1) / GFX_MUSIC_VISUALIZER_HEIGHT
         || (pitch & 3) || ((uintptr_t)pixels & 3)) return false;
   history.bytes = retro_spsc_read_begin_pair(&v->rows, &first,
         &history.first_bytes, &second, MUSIC_QUEUE_BYTES);
   history.first = (const uint8_t*)first;
   history.second = (const uint8_t*)second;
   if (history.bytes)
      sequence = gfx_music_row_at(&history, history.bytes - MUSIC_ROW_BYTES)->sequence;
   if (v->shown && v->shown_sequence == sequence)
   {
      if (history.bytes) retro_spsc_read_end(&v->rows, 0);
      return false;
   }
   gfx_music_render(v, pixels, pitch, rgba, &history);
   if (history.bytes)
      retro_spsc_read_end(&v->rows, history.bytes > MUSIC_HISTORY * MUSIC_ROW_BYTES
            ? history.bytes - MUSIC_HISTORY * MUSIC_ROW_BYTES : 0);
   v->shown_sequence = sequence;
   v->shown = true;
   return true;
}

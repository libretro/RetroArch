#ifndef GFX_MENU_RIBBON_H
#define GFX_MENU_RIBBON_H

#include <math.h>
#include <retro_math.h>
#include <stdint.h>

#define MENU_RIBBON_PARTICLES 48
#define MENU_RIBBON_UNIFORMS 65

typedef struct menu_ribbon_particle
{
   float x, y, vx, vy, age, life, depth, phase;
} menu_ribbon_particle_t;

typedef struct menu_ribbon
{
   double last_time;
   double time;
   float header[4];
   float data[MENU_RIBBON_UNIFORMS][4];
   menu_ribbon_particle_t particles[MENU_RIBBON_PARTICLES];
   float remainder;
   uint32_t random;
} menu_ribbon_t;

static float menu_ribbon_random(menu_ribbon_t *r)
{
   r->random = r->random * 1664525u + 1013904223u;
   return (float)(r->random >> 8) * (1.0f / 16777216.0f);
}

static void menu_ribbon_surface(float x, float z, float time,
      float *position, float *lighting)
{
   float a = x * 1.45f + time * 0.08f;
   float b = z * 1.75f + x * (1.2f + 0.45f * z) - time * 0.06f;
   float c = z * (5.8f + 1.2f * z) + x * 0.75f - time * 0.09f;
   float d = z * 10.0f + x * (2.2f - 0.6f * z) + time * 0.075f;
   float width = 0.13f + 0.05f * cosf(a);
   float y = -0.28f + 0.12f * sinf(a) + width * cosf(b) * cosf(c)
         + 0.04f * sinf(d) + 0.06f * z;
   float dx = 0.174f * cosf(a) - 0.0725f * sinf(a) * cosf(b) * cosf(c)
         - width * ((1.2f + 0.45f * z) * sinf(b) * cosf(c)
            + 0.75f * cosf(b) * sinf(c))
         + 0.04f * (2.2f - 0.6f * z) * cosf(d);
   float dz = -width * ((1.75f + 0.45f * x) * sinf(b) * cosf(c)
         + (5.8f + 2.4f * z) * cosf(b) * sinf(c))
         + 0.04f * (10.0f - 0.6f * x) * cosf(d) + 0.06f;
   float w = 1.0f + 0.10f * z;
   float nx = dx, ny = -1.12f, nz = 1.12f * dz;
   float ex = -0.112f * x, ey = -0.10f * y, ez = -w;
   position[0] = 1.12f * x;
   position[1] = y;
   position[2] = 0.0f;
   position[3] = w;
   lighting[0] = (nx * ex + ny * ey + nz * ez)
         / sqrtf((nx * nx + ny * ny + nz * nz) * (ex * ex + ey * ey + ez * ez));
   lighting[1] = (dz * w - 0.10f * y) / (w * w);
   lighting[2] = z;
   lighting[3] = 0.65f + 0.35f * cosf(z * 28.0f);
}

static void menu_ribbon_vertex(const float *data, const float *p,
      float *position, float *lighting)
{
   if (p[1] > 1.5f)
   {
      unsigned id = (unsigned)((p[1] - 2.0f) * 0.25f);
      const float *star = data + (16 + id) * 4;
      float cy = (p[1] - 2.0f - id * 4.0f) * 2.0f - 1.0f;
      float radius = star[2] * data[257];
      position[0] = star[0] + p[0] * radius * data[256];
      position[1] = star[1] + cy * radius;
      position[2] = 0.0f;
      position[3] = 1.0f;
      lighting[0] = p[0];
      lighting[1] = cy;
      lighting[2] = 2.0f;
      lighting[3] = star[3] * data[258];
   }
   else
   {
      menu_ribbon_surface(p[0], p[1], data[0], position, lighting);
      lighting[3] *= data[258];
   }
}

static void menu_ribbon_emit(menu_ribbon_t *r, menu_ribbon_particle_t *p)
{
   float x = menu_ribbon_random(r) * 2.0f - 1.0f;
   float z = menu_ribbon_random(r) * 1.8f - 0.9f;
   float position[4], lighting[4];
   menu_ribbon_surface(x, z, (float)(r->time * 0.6), position, lighting);
   p->x = position[0] / position[3];
   p->y = position[1] / position[3]
         + (menu_ribbon_random(r) - 0.5f) * 0.055f;
   p->vx = (menu_ribbon_random(r) - 0.40f) * 0.008f;
   p->vy = (menu_ribbon_random(r) - 0.55f) * 0.010f;
   p->life = 10.0f + menu_ribbon_random(r) * 18.0f;
   p->age = 0.0f;
   p->depth = menu_ribbon_random(r);
   p->phase = menu_ribbon_random(r) * 6.2831853f;
}

static void menu_ribbon_step(menu_ribbon_t *r)
{
   unsigned i;
   r->time += 1.0 / 60.0;
   for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
   {
      menu_ribbon_particle_t *p = &r->particles[i];
      p->age += 1.0f / 60.0f;
      if (p->age >= p->life)
         menu_ribbon_emit(r, p);
      p->x += p->vx * (1.0f / 60.0f);
      p->y += p->vy * (1.0f / 60.0f);
   }
}

static void menu_ribbon_update(menu_ribbon_t *r, double now,
      unsigned width, unsigned height, float alpha)
{
   unsigned i, steps;
   float (*data)[4] = r->data;
   float elapsed;
   if (!r->random)
   {
      r->random = 0x584d4233u;
      for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
      {
         menu_ribbon_particle_t *p = &r->particles[i];
         menu_ribbon_emit(r, p);
         p->age = menu_ribbon_random(r) * p->life;
         p->x += p->vx * p->age;
         p->y += p->vy * p->age;
      }
      r->last_time = now;
   }
   elapsed = (float)(now - r->last_time);
   r->last_time = now;
   if (elapsed < 0.0f)
      elapsed = 0.0f;
   if (elapsed > 1.0f / 15.0f)
      elapsed = 1.0f / 15.0f;
   r->remainder += elapsed;
   steps = (unsigned)(r->remainder * 60.0f);
   if (steps > 4)
      steps = 4;
   r->remainder -= steps * (1.0f / 60.0f);
   for (i = 0; i < steps; i++)
      menu_ribbon_step(r);
   for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
   {
      const menu_ribbon_particle_t *p = &r->particles[i];
      float fade = p->age / p->life;
      float pulse = sinf(p->age * (1.2f + 1.7f * p->depth) + p->phase)
            * sinf(p->age * 2.3f + p->phase * 3.1f);
      float sparkle = pulse > 0.0f ? pulse * pulse : 0.0f;
      sparkle *= sparkle;
      fade = fade < 0.5f ? fade * 6.0f : (1.0f - fade) * 5.0f;
      if (fade > 1.0f)
         fade = 1.0f;
      fade *= fade * (3.0f - 2.0f * fade);
      data[16 + i][0] = p->x;
      data[16 + i][1] = p->y;
      data[16 + i][2] = 1.25f + 3.8f * p->depth * p->depth * p->depth;
      data[16 + i][3] = fade * (0.16f + 0.22f * p->depth + 2.0f * sparkle);
   }
   r->data[0][0] = (float)(r->time * 0.6);
   r->data[64][0] = width ? (float)height / width : 1.0f;
   r->data[64][1] = height ? 2.0f / height : 0.0f;
   r->data[64][2] = alpha;
}

#endif

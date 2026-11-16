#ifndef GFX_MENU_RIBBON_H
#define GFX_MENU_RIBBON_H

#include <math.h>
#include <retro_inline.h>
#include <retro_math.h>
#include <stdint.h>

#define MENU_RIBBON_PARTICLES 48
#define MENU_RIBBON_UNIFORMS 50
/* The sheet's phase a second of menu time */
#define MENU_RIBBON_SPEED 1.2

typedef struct menu_ribbon_particle
{
   double born;
   float x, y, x0, y0, vx, vy, age, life, depth, phase;
} menu_ribbon_particle_t;

/* A function of the menu's clock alone: particles are reborn in the
 * order their lives end, each drawing from the one sequence, so frame
 * rate and pacing never change what is shown at a given time. */
typedef struct menu_ribbon
{
   double last_time;
   double time;
   float header[4];
   float data[MENU_RIBBON_UNIFORMS][4];
   menu_ribbon_particle_t particles[MENU_RIBBON_PARTICLES];
   uint32_t random;
} menu_ribbon_t;

static INLINE float menu_ribbon_random(menu_ribbon_t *r)
{
   r->random = r->random * 1664525u + 1013904223u;
   return (float)(r->random >> 8) * (1.0f / 16777216.0f);
}

static INLINE void menu_ribbon_surface(float x, float z, float time,
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

static INLINE void menu_ribbon_vertex(const float *data, const float *p,
      float *position, float *lighting)
{
   if (p[1] > 1.5f)
   {
      unsigned id = (unsigned)((p[1] - 2.0f) * 0.25f);
      const float *star = data + (1 + id) * 4;
      float cy = (p[1] - 2.0f - id * 4.0f) * 2.0f - 1.0f;
      float radius = star[2] * data[197];
      position[0] = star[0] + p[0] * radius * data[196];
      position[1] = star[1] + cy * radius;
      position[2] = 0.0f;
      position[3] = 1.0f;
      lighting[0] = p[0];
      lighting[1] = cy;
      lighting[2] = 2.0f;
      lighting[3] = star[3] * data[198];
   }
   else
   {
      menu_ribbon_surface(p[0], p[1], data[0], position, lighting);
      lighting[3] *= data[198];
   }
}

static INLINE void menu_ribbon_spawn(menu_ribbon_t *r,
      menu_ribbon_particle_t *p, double born)
{
   float x = menu_ribbon_random(r) * 2.0f - 1.0f;
   float z = menu_ribbon_random(r) * 1.8f - 0.9f;
   float position[4], lighting[4];
   menu_ribbon_surface(x, z, (float)(born * MENU_RIBBON_SPEED),
         position, lighting);
   p->born  = born;
   p->x0    = position[0] / position[3];
   p->y0    = position[1] / position[3]
         + (menu_ribbon_random(r) - 0.5f) * 0.055f;
   p->vx    = (menu_ribbon_random(r) - 0.40f) * 0.008f;
   p->vy    = (menu_ribbon_random(r) - 0.55f) * 0.010f;
   p->life  = 10.0f + menu_ribbon_random(r) * 18.0f;
   p->depth = menu_ribbon_random(r);
   p->phase = menu_ribbon_random(r) * 6.2831853f;
}

static INLINE void menu_ribbon_update(menu_ribbon_t *r, double now,
      unsigned width, unsigned height, float alpha)
{
   unsigned i;
   double elapsed;
   float (*data)[4] = r->data;
   if (!r->random)
   {
      r->random    = 0x584d4233u;
      r->time      = 0.0;
      r->last_time = now;
      for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
      {
         menu_ribbon_particle_t *p = &r->particles[i];
         menu_ribbon_spawn(r, p, 0.0);
         p->born = -(double)(menu_ribbon_random(r) * p->life);
      }
   }
   /* Clamped so a stall resumes where it stopped */
   elapsed      = now - r->last_time;
   r->last_time = now;
   if (elapsed < 0.0)
      elapsed = 0.0;
   if (elapsed > 1.0 / 15.0)
      elapsed = 1.0 / 15.0;
   r->time     += elapsed;
   for (;;)
   {
      menu_ribbon_particle_t *next = NULL;
      double end                   = r->time;
      for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
      {
         menu_ribbon_particle_t *p = &r->particles[i];
         if (p->born + p->life <= end)
         {
            end  = p->born + p->life;
            next = p;
         }
      }
      if (!next)
         break;
      menu_ribbon_spawn(r, next, end);
   }
   for (i = 0; i < MENU_RIBBON_PARTICLES; i++)
   {
      menu_ribbon_particle_t *p = &r->particles[i];
      float fade, pulse, sparkle;
      p->age  = (float)(r->time - p->born);
      p->x    = p->x0 + p->vx * p->age;
      p->y    = p->y0 + p->vy * p->age;
      fade    = p->age / p->life;
      pulse   = sinf(p->age * (1.2f + 1.7f * p->depth) + p->phase)
            * sinf(p->age * 2.3f + p->phase * 3.1f);
      sparkle = pulse > 0.0f ? pulse * pulse : 0.0f;
      sparkle *= sparkle;
      fade = fade < 0.5f ? fade * 6.0f : (1.0f - fade) * 5.0f;
      if (fade > 1.0f)
         fade = 1.0f;
      fade *= fade * (3.0f - 2.0f * fade);
      data[1 + i][0] = p->x;
      data[1 + i][1] = p->y;
      data[1 + i][2] = 1.25f + 3.8f * p->depth * p->depth * p->depth;
      data[1 + i][3] = fade * (0.16f + 0.22f * p->depth + 2.0f * sparkle);
   }
   data[0][0]  = (float)(r->time * MENU_RIBBON_SPEED);
   data[49][0] = width ? (float)height / width : 1.0f;
   data[49][1] = height ? 2.0f / height : 0.0f;
   data[49][2] = alpha;
}

#endif

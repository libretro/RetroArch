#include <stdio.h>
#include <string.h>

#include "gfx/video_driver.h"
#include "gfx/gfx_display.h"

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

#include "uniforms.inc"

static unsigned failures;
static gfx_display_t display;

#define CHECK(condition, message) \
   do { if (!(condition)) { puts(message); failures++; } } while (0)

#if TEST_GL3
struct video_coords *gfx_display_effect_coords(gfx_display_t *p_disp)
{
   static struct video_coords coords;
   (void)p_disp;
   return &coords;
}

static void upload(gfx_display_ctx_draw_t *draw, gfx_display_t *p_disp)
{
   struct { math_matrix_4x4 mvp_no_rot; } state, *gl = &state;
   unsigned video_width = 1280, video_height = 720;
   float t = p_disp->effect_time;
   float yflip = 0.0f;
   memset(&state, 0x55, sizeof(state));
#include "upload.inc"
}
#else
typedef struct { uniform_t ubo_values; } driver_t;
static driver_t state;

static void upload(gfx_display_ctx_draw_t *draw, gfx_display_t *p_disp)
{
   driver_t *driver = &state;
   (void)draw;
#include "upload.inc"
   draw->backend_data = &driver->ubo_values;
   draw->backend_data_size = sizeof(driver->ubo_values);
}
#endif

int main(void)
{
   const float opacity[] = {1.0f, 0.375f, 0.0f, 1.0f};
   gfx_display_ctx_draw_t draw;
   float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
   float uploaded[20];
   unsigned i, program;
   memset(&draw, 0, sizeof(draw));
   draw.dims = VIDEO_SCALE_PACK(1280, 720);
   display.effect_time = 32.75f;
   for (program = VIDEO_SHADER_MENU_2; program <= VIDEO_SHADER_MENU; program++)
      for (i = 0; i < sizeof(opacity) / sizeof(opacity[0]); i++)
      {
         color[3] = opacity[i];
         draw.color = i ? color : NULL;
#if TEST_GL3
         draw.pipeline_id = VIDEO_SHADER_MENU_3;
         upload(&draw, &display);
#endif
         draw.pipeline_id = program;
         upload(&draw, &display);
#if TEST_GL3
         CHECK(draw.backend_data_size >= 3 * sizeof(float),
               "FAIL: ribbon upload omits opacity");
         memcpy(uploaded, draw.backend_data, 4 * sizeof(float));
         CHECK(uploaded[0] == display.effect_time, "FAIL: ribbon time");
         CHECK(uploaded[1] == 1.0f, "FAIL: ribbon orientation");
         CHECK(uploaded[2] == opacity[i], "FAIL: ribbon opacity");
#else
         CHECK(draw.backend_data_size == sizeof(uploaded),
               "FAIL: shader uniform buffer size");
         memcpy(uploaded, draw.backend_data, sizeof(uploaded));
         CHECK(uploaded[18] == display.effect_time, "FAIL: ribbon time");
         CHECK(uploaded[19] == opacity[i], "FAIL: ribbon opacity");
#endif
      }
   return failures != 0;
}

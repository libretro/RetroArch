/* integer_scale_axis_test.c -- the viewport integer scaling picks.
 *
 * Runs video_viewport_get_scaled_integer() from gfx/video_driver.c
 * (pulled out by extract_integer.awk) against a stubbed frame cache
 * and checks the drawn size it hands back:
 *
 *   1. Y+X is integer on both axes. The width is a whole multiple of
 *      the frame's width, the height of the frame's height. The X axis
 *      used to be multiplied from the aspect-corrected width instead
 *      (256x224 at 64:49 is 293 wide), so 1080p gave 1172x896 - a 4.58x
 *      horizontal scale, uneven pixel columns, and the fraction the
 *      statistics overlay showed. The 240p test suite checkerboard case
 *      from the report is the first row of the table.
 *   2. The X-only modes are held to the same rule for the width.
 *   3. Rotated content: the frame's height is the displayed width.
 *   4. Square pixels: the aspect-corrected width equals the frame width,
 *      so these passed before as well; they pin that the fix leaves
 *      the common case where it was.
 *   5. Hi-res sources on the half-step modes may use half steps, so
 *      the width is a whole multiple of half the frame width.
 *
 * A pick that falls back to non-integer scaling (the Smart mode's
 * margin rule, no room for 1x) is reported by the stub and not held
 * to the multiple rule. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include "gfx/video_defines.h"

/* The slice of video_driver_state_t the scaler reads. */
typedef struct
{
   struct
   {
      struct
      {
         unsigned base_width;
         unsigned base_height;
      } geometry;
   } av_info;
   float    aspect;
   unsigned scale_dims;
} video_driver_state_t;

#define VIDEO_DRIVER_ASPECT_RATIO(video_st) ((video_st)->aspect)

static unsigned stub_frame_dims;
static unsigned fallbacks;

static void frame_cache_peek(const void **data, unsigned *dims,
      size_t *pitch)
{
   *data  = NULL;
   *dims  = stub_frame_dims;
   *pitch = 0;
}

void video_viewport_get_scaled_aspect2(struct video_viewport *vp,
      unsigned dims, bool y_down, float device_aspect,
      float desired_aspect)
{
   (void)y_down; (void)device_aspect; (void)desired_aspect;
   vp->dims = dims;
   vp->pos  = 0;
   fallbacks++;
}

#include "integer_scale_driver.h"

static unsigned failures;
static unsigned checked;

static const char *axis_name(unsigned a)
{
   switch (a)
   {
      case VIDEO_SCALE_INTEGER_AXIS_Y:            return "Y";
      case VIDEO_SCALE_INTEGER_AXIS_Y_X:          return "Y+X";
      case VIDEO_SCALE_INTEGER_AXIS_Y_XHALF:      return "Y+X.5";
      case VIDEO_SCALE_INTEGER_AXIS_YHALF_XHALF:  return "Y.5+X.5";
      case VIDEO_SCALE_INTEGER_AXIS_X:            return "X";
      case VIDEO_SCALE_INTEGER_AXIS_XHALF:        return "X.5";
   }
   return "?";
}

static const char *scaling_name(unsigned s)
{
   switch (s)
   {
      case VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE: return "underscale";
      case VIDEO_SCALE_INTEGER_SCALING_OVERSCALE:  return "overscale";
      case VIDEO_SCALE_INTEGER_SCALING_SMART:      return "smart";
   }
   return "?";
}

/* Runs the scaler once; returns 0 if it fell back to non-integer. */
static int run(unsigned vp_w, unsigned vp_h,
      unsigned fw, unsigned fh, float aspect,
      unsigned axis, unsigned scaling, unsigned rotation,
      unsigned *out_w, unsigned *out_h)
{
   video_driver_state_t st;
   struct video_vp_param_snap ps;
   struct video_viewport vp;
   unsigned before = fallbacks;

   memset(&st, 0, sizeof(st));
   memset(&ps, 0, sizeof(ps));
   memset(&vp, 0, sizeof(vp));

   st.av_info.geometry.base_width  = fw;
   st.av_info.geometry.base_height = fh;
   st.aspect                       = aspect;

   ps.aspect           = aspect;
   ps.bias_x           = 0.5f;
   ps.bias_y           = 0.5f;
   ps.rotation         = rotation;
   ps.aspect_ratio_idx = ASPECT_RATIO_CORE;
   ps.si_scaling       = scaling;
   ps.si_axis          = axis;
   ps.scale_integer    = true;

   stub_frame_dims     = VIDEO_SCALE_PACK(fw, fh);

   video_viewport_get_scaled_integer(&st, &ps, &vp,
         VIDEO_SCALE_PACK(vp_w, vp_h), aspect, true, true, rotation);

   *out_w = VIDEO_SCALE_W(vp.dims);
   *out_h = VIDEO_SCALE_H(vp.dims);
   return fallbacks == before;
}

/* Holds one pick to "width is a multiple of step_w, height of step_h". */
static void expect_multiple(const char *lane,
      unsigned vp_w, unsigned vp_h,
      unsigned fw, unsigned fh, float aspect,
      unsigned axis, unsigned scaling, unsigned rotation,
      unsigned step_w, unsigned step_h)
{
   unsigned w = 0, h = 0;

   if (!run(vp_w, vp_h, fw, fh, aspect, axis, scaling, rotation, &w, &h))
      return;

   checked++;

   if (w == 0 || h == 0 || w % step_w || (step_h && h % step_h))
   {
      printf("[fail] %s: %ux%u frame %ux%u aspect %.5f rot %u %s %s"
            " -> %ux%u (scale %.2f/%.2f), expected multiples of %u/%u\n",
            lane, vp_w, vp_h, fw, fh, aspect, rotation,
            axis_name(axis), scaling_name(scaling), w, h,
            (double)w / (rotation % 2 ? fh : fw),
            (double)h / (rotation % 2 ? fw : fh),
            step_w, step_h);
      failures++;
   }
}

static const unsigned viewports[][2] = {
   { 1920, 1080 }, { 1280,  720 }, { 2560, 1440 }, { 3840, 2160 },
   { 1366,  768 }, { 1280, 1024 }, { 1600, 1200 }, {  800,  600 }
};
#define N_VIEWPORTS (sizeof(viewports) / sizeof(viewports[0]))

/* 1. the reported case, exactly */
static void lane_report(void)
{
   unsigned w = 0, h = 0;
   float aspect = 64.0f / 49.0f; /* 1.30612, Snes9x core provided */

   if (!run(1920, 1080, 256, 224, aspect, VIDEO_SCALE_INTEGER_AXIS_Y_X,
            VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE, 0, &w, &h))
   {
      printf("[fail] report: 1080p 256x224 Y+X underscale fell back"
            " to non-integer scaling\n");
      failures++;
      return;
   }
   checked++;
   /* 4x height is 896; the nearest whole width to 64:49 is 5x = 1280 */
   if (w != 1280 || h != 896)
   {
      printf("[fail] report: 1080p 256x224 Y+X underscale gave %ux%u"
            " (scale %.2f/%.2f), expected 1280x896 (5.00/4.00)\n",
            w, h, w / 256.0, h / 224.0);
      failures++;
   }
}

/* 1, 2, 4: every axis mode that touches X, every scaling, sweep */
static void lane_axes(void)
{
   static const struct { unsigned w, h; float aspect; } frames[] = {
      { 256, 224, 64.0f / 49.0f },  /* SNES, 8:7 PAR  */
      { 256, 240, 4.0f / 3.0f   },  /* NES/240p suite */
      { 320, 224, 4.0f / 3.0f   },  /* Genesis        */
      { 160, 144, 10.0f / 9.0f  },  /* GB, square     */
      { 240, 160, 3.0f / 2.0f   },  /* GBA, square    */
      { 256, 224, 8.0f / 7.0f   }   /* SNES, square   */
   };
   size_t f, v;
   unsigned axis, scaling;

   for (f = 0; f < sizeof(frames) / sizeof(frames[0]); f++)
      for (v = 0; v < N_VIEWPORTS; v++)
         for (axis = VIDEO_SCALE_INTEGER_AXIS_Y_X;
               axis < VIDEO_SCALE_INTEGER_AXIS_LAST; axis++)
            for (scaling = 0; scaling < VIDEO_SCALE_INTEGER_SCALING_LAST;
                  scaling++)
            {
               /* the X-only modes derive the height from the aspect,
                * so only the width is held to a multiple there */
               unsigned step_h = (axis >= VIDEO_SCALE_INTEGER_AXIS_X)
                     ? 0 : frames[f].h;
               expect_multiple("axes", viewports[v][0], viewports[v][1],
                     frames[f].w, frames[f].h, frames[f].aspect,
                     axis, scaling, 0, frames[f].w, step_h);
            }
}

/* 3. rotated: the frame's height is what spans the display's width */
static void lane_rotated(void)
{
   size_t v;
   unsigned scaling;

   for (v = 0; v < N_VIEWPORTS; v++)
      for (scaling = 0; scaling < VIDEO_SCALE_INTEGER_SCALING_LAST;
            scaling++)
      {
         /* a 256x224 frame on its side, core aspect 7:8 PAR */
         expect_multiple("rotated", viewports[v][0], viewports[v][1],
               256, 224, 49.0f / 64.0f,
               VIDEO_SCALE_INTEGER_AXIS_Y_X, scaling, 1, 224, 256);
         /* a 320x240 arcade frame on its side, 3:4 */
         expect_multiple("rotated", viewports[v][0], viewports[v][1],
               320, 240, 3.0f / 4.0f,
               VIDEO_SCALE_INTEGER_AXIS_Y_X, scaling, 1, 240, 320);
      }
}

/* 5. hi-res on the half-step modes: half the frame width is the step */
static void lane_hires(void)
{
   size_t v;
   unsigned scaling;

   for (v = 0; v < N_VIEWPORTS; v++)
      for (scaling = 0; scaling < VIDEO_SCALE_INTEGER_SCALING_LAST;
            scaling++)
      {
         expect_multiple("hires", viewports[v][0], viewports[v][1],
               512, 448, 64.0f / 49.0f,
               VIDEO_SCALE_INTEGER_AXIS_Y_XHALF, scaling, 0, 256, 448);
         expect_multiple("hires", viewports[v][0], viewports[v][1],
               512, 224, 64.0f / 49.0f,
               VIDEO_SCALE_INTEGER_AXIS_Y_X, scaling, 0, 512, 224);
      }
}

int main(void)
{
   lane_report();
   lane_axes();
   lane_rotated();
   lane_hires();

   if (checked < 100)
   {
      printf("[fail] only %u picks were integer; the sweep is not"
            " exercising the scaler\n", checked);
      failures++;
   }

   if (failures)
   {
      printf("integer_scale_axis_test: %u failure(s) in %u checks\n",
            failures, checked);
      return 1;
   }
   printf("[pass] integer_scale_axis_test: %u picks checked,"
         " %u non-integer fallbacks skipped\n", checked, fallbacks);
   return 0;
}

/* What the stand-in XeSS library (fake_xess.c) saw: read by a test
 * through its exported fake_xess_record(). Plain C, no SDK. */
#ifndef FAKE_XESS_H
#define FAKE_XESS_H

#include <stdint.h>

#define FAKE_XESS_MAX_CALLS 8

typedef struct fake_xess_init
{
   int vk;                      /* Vulkan's init, or Direct3D 12's */
   void *context;
   uint32_t out_w, out_h;
   int quality;
   uint32_t flags;
   uint32_t node_mask;
} fake_xess_init_t;

typedef struct fake_xess_image
{
   int present;
   uint64_t image;              /* Direct3D 12: the resource */
   uint64_t view;
   uint32_t aspect, width, height;
   int format;
} fake_xess_image_t;

typedef struct fake_xess_execute
{
   int vk;
   void *context;
   void *command_list;
   fake_xess_image_t color, velocity, depth, exposure, mask, output;
   float jitter_x, jitter_y, exposure_scale;
   uint32_t reset, input_w, input_h;
} fake_xess_execute_t;

typedef struct fake_xess_record
{
   unsigned creates, inits, executes, destroys, pipelines;
   fake_xess_init_t init[FAKE_XESS_MAX_CALLS];
   fake_xess_execute_t execute[FAKE_XESS_MAX_CALLS];
   void *create_device, *create_instance, *create_physical_device;
   float velocity_scale_x, velocity_scale_y;
   float jitter_scale_x, jitter_scale_y;
   float exposure_multiplier;
   int log_level;
   /* the test's knobs */
   int create_fails;            /* XESS_RESULT_ERROR_UNSUPPORTED_DEVICE */
   int init_fails;
   int execute_fails;
   int build_blocking;          /* what the last build_pipelines was asked */
   int build_pending;           /* get_pipeline_build_status: this many more calls in progress */
} fake_xess_record_t;

#endif

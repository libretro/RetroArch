/* What fake_ngx.c records, read by the test through fake_ngx_record(),
 * resolved from the loaded library */
#ifndef FAKE_NGX_H
#define FAKE_NGX_H

#define FAKE_NGX_MAX 16

typedef struct fake_ngx_create
{
   unsigned width, height, out_width, out_height;
   int quality;
   int flags;
   unsigned node_mask;
   void *cmd;
} fake_ngx_create_t;

typedef struct fake_ngx_eval
{
   void *color, *output, *depth, *motion_vectors, *exposure, *transparency;
   void *cmd;
   float jitter_x, jitter_y, mv_scale_x, mv_scale_y;
   float pre_exposure, exposure_scale, frame_time;
   int reset;
   unsigned subrect_w, subrect_h;
   int feature;
} fake_ngx_eval_t;

typedef struct fake_ngx_record
{
   fake_ngx_create_t create[FAKE_NGX_MAX];
   fake_ngx_eval_t eval[FAKE_NGX_MAX];
   void *init_device;
   unsigned creates, evals, releases, optimal_calls;
   unsigned init_calls, shutdown_calls, params_destroyed;
   int last_released;
   int init_path0;
   int shutdown_path0;   /* the path list Init was given, read again at Shutdown1 */
   /* Knobs the test turns */
   int unavailable;
   int create_fails;
   int eval_fails;
} fake_ngx_record_t;

#endif

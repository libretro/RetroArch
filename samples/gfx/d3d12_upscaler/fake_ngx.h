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
   unsigned preset;   /* the Quality mode's render preset hint */
   int feature;
   /* Neural Rendering's (feature 18) tuning */
   unsigned nr_preset, nr_style, nr_auto_mask;
   float nr_intensity, nr_structure, nr_tone, nr_skin;
} fake_ngx_create_t;

typedef struct fake_ngx_eval
{
   void *color, *output, *depth, *motion_vectors, *exposure, *transparency;
   void *cmd;
   float jitter_x, jitter_y, mv_scale_x, mv_scale_y;
   float pre_exposure, exposure_scale, frame_time, sharpness;
   int reset;
   unsigned subrect_w, subrect_h;
   int feature;
   /* Neural Rendering's: the guides' subrects, the depth's sense */
   int nr;
   unsigned depth_w, depth_h, mv_w, mv_h;
   int depth_inverted;
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
   int optiscaler;    /* the tables carry OptiScaler's key, as its stand-in's do */
   int eval_fails;
   int nr_missing;    /* the core has no Neural Rendering, as before its driver */
} fake_ngx_record_t;

#endif

/* gfx_ctx_drm_present_immediate() against the condition
 * gfx_ctx_drm_queue_flip() queues an async flip under (shipping.inc,
 * cut by run.py), over every state the context can be in. */
#include <stdio.h>
#include <stddef.h>
#include <boolean.h>

typedef struct
{
   int interval;
   bool async_flip;
   bool async_flip_refused;
   bool video_vsync;
} gfx_ctx_drm_data_t;

#include "shipping.inc"

int main(void)
{
   unsigned i;
   unsigned failures = 0;
   for (i = 0; i < 24; i++)
   {
      gfx_ctx_drm_data_t d;
      gfx_ctx_drm_data_t *drm = &d;
      bool flips_async, immediate;
      d.interval           = (int)(i % 3);
      d.async_flip         = (i / 3) & 1;
      d.video_vsync        = (i / 6) & 1;
      d.async_flip_refused = (i / 12) & 1;
      /* a flip the kernel refused async waits for its vblank */
      flips_async = FLIP_IS_ASYNC(drm) && !d.async_flip_refused;
      immediate   = gfx_ctx_drm_present_immediate(&d);
      if (immediate != flips_async)
      {
         failures++;
         fprintf(stderr, "FAIL interval %d async %d vsync %d refused %d: "
               "immediate %d, the flip is async %d\n", d.interval,
               d.async_flip, d.video_vsync, d.async_flip_refused,
               immediate, flips_async);
      }
   }
   if (gfx_ctx_drm_present_immediate(NULL))
   {
      failures++;
      fprintf(stderr, "FAIL no context is immediate\n");
   }
   if (failures)
      return 1;
   printf("PASS KMS present_immediate is the async page flip's condition\n");
   return 0;
}

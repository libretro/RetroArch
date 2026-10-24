#include <math.h>
#include <string.h>
#include "gfx_media_display.h"
#include "video_defines.h"

bool gfx_media_display_source(const gfx_media_playback_state_t *state,
      unsigned *dims, float *hz)
{
   double rate;
   if (dims) *dims = 0;
   if (hz) *hz = 0.0f;
   if (!state || !state->cadence_num || !state->cadence_den
         || !VIDEO_SCALE_W(state->dims) || VIDEO_SCALE_H(state->dims) <= 4
         || state->seeking || state->seek_failed || state->cadence_unstable)
      return false;
   rate = (double)state->cadence_num / state->cadence_den;
   if (!(rate >= 5.0 && rate <= 240.0)) return false;
   if (dims) *dims = state->dims;
   if (hz) *hz = (float)rate;
   return true;
}

float gfx_media_display_rate(const video_display_config_t *modes,
      unsigned count, unsigned dims, unsigned bpp,
      unsigned cadence_num, unsigned cadence_den, float current_hz)
{
   double fps, best_error = 1.0, best_distance = 1.0e30;
   float best = 0.0f;
   unsigned i;
   if (!modes || !dims || !cadence_num || !cadence_den)
      return 0.0f;
   fps = (double)cadence_num / cadence_den;
   if (!(fps >= 5.0 && fps <= 240.0))
      return 0.0f;
   for (i = 0; i < count; i++)
   {
      double hz, error, distance;
      unsigned multiple;
      if (modes[i].dims != dims || modes[i].interlaced || modes[i].dblscan
            || (bpp && modes[i].bpp && modes[i].bpp != bpp))
         continue;
      hz = modes[i].refreshrate_float > 0.0f
         ? modes[i].refreshrate_float : modes[i].refreshrate;
      if (!(hz >= 20.0 && hz <= 1000.0))
         continue;
      multiple = (unsigned)(hz / fps + 0.5);
      if (!multiple)
         continue;
      error = fabs(hz - fps * multiple) / hz;
      if (error > 0.0015)
         continue;
      distance = current_hz > 0.0f ? fabs(hz - current_hz) : hz;
      if (error + 0.000001 < best_error
            || (fabs(error - best_error) <= 0.000001
               && (distance < best_distance
                  || (distance == best_distance && hz < best))))
      {
         best = (float)hz;
         best_error = error;
         best_distance = distance;
      }
   }
   return best;
}

void gfx_media_display_owner_reset(gfx_media_display_owner_t *owner)
{
   if (owner->lease && owner->server && owner->server->end_media_refresh)
      owner->server->end_media_refresh(owner->data, owner->lease);
   memset(owner, 0, sizeof(*owner));
}

void gfx_media_display_owner_update(gfx_media_display_owner_t *owner,
      const video_display_server_t *server, void *data,
      unsigned num, unsigned den, bool output_changed)
{
   if (!num || !den || !server || !data)
   {
      gfx_media_display_owner_reset(owner);
      return;
   }
   if (owner->server == server && owner->data == data)
   {
      if (!output_changed && owner->num == num && owner->den == den)
         return;
      if (owner->lease && server->media_refresh_current
            && server->media_refresh_current(data, owner->lease, num, den))
      {
         owner->num = num;
         owner->den = den;
         return;
      }
   }
   gfx_media_display_owner_reset(owner);
   owner->server = server;
   owner->data = data;
   owner->num = num;
   owner->den = den;
   if (server->begin_media_refresh && server->end_media_refresh)
      owner->lease = server->begin_media_refresh(data, num, den);
}

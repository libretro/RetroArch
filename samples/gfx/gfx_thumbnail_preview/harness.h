#ifndef HARNESS_H
#define HARNESS_H
typedef struct {
   int texture_uploads, texture_unloads, fade_pushes, still_loads;
   /* HDR: the stub driver offers half floats while hdr_output is set;
    * fp16_uploads counts textures loaded as them, fp16_r is the red
    * of the centre texel of the last one */
   int hdr_output, fp16_uploads;
   unsigned short fp16_r;
   int audio_streams, audio_stops, audio_stalls, audio_avail_raises;
   size_t last_audio_bytes;
   int force_preview_audio;
   unsigned last_tex_w, last_tex_h;
   /* Still sets: a threaded task queue to batch on, the thumbnail
    * settings to request under, and what went to the queue */
   int threaded_queue, thumb_cfg, still_sets, set_items, set_max;
} harness_probe_t;
extern harness_probe_t hp;
#endif

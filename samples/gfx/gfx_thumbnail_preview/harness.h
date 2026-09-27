#ifndef HARNESS_H
#define HARNESS_H
typedef struct {
   int texture_uploads, texture_unloads, fade_pushes, still_loads;
   int audio_streams, audio_stops, audio_stalls, audio_avail_raises;
   size_t last_audio_bytes;
   int force_preview_audio;
   unsigned last_tex_w, last_tex_h;
} harness_probe_t;
extern harness_probe_t hp;
#endif

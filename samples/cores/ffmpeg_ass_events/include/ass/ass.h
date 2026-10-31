/* The part of libass the ffmpeg core uses, for the fixtures. */
#ifndef FIXTURE_ASS_H
#define FIXTURE_ASS_H

#include <stdarg.h>
#include <stdint.h>

typedef struct ASS_Library  ASS_Library;
typedef struct ASS_Renderer ASS_Renderer;
typedef struct ASS_Track    { int n_events; } ASS_Track;

typedef struct ASS_Image
{
   int w, h, stride;
   unsigned char *bitmap;
   uint32_t color;
   int dst_x, dst_y;
   struct ASS_Image *next;
} ASS_Image;

typedef enum { ASS_HINTING_NONE, ASS_HINTING_LIGHT } ASS_Hinting;

ASS_Library *ass_library_init(void);
void ass_library_done(ASS_Library *library);
void ass_set_message_cb(ASS_Library *library,
      void (*cb)(int level, const char *fmt, va_list args, void *data),
      void *data);
void ass_set_extract_fonts(ASS_Library *library, int extract);
void ass_add_font(ASS_Library *library, const char *name,
      const char *data, int data_size);
ASS_Renderer *ass_renderer_init(ASS_Library *library);
void ass_renderer_done(ASS_Renderer *renderer);
void ass_set_frame_size(ASS_Renderer *renderer, int w, int h);
void ass_set_hinting(ASS_Renderer *renderer, ASS_Hinting hinting);
void ass_set_fonts(ASS_Renderer *renderer, const char *default_font,
      const char *default_family, int dfp, const char *config, int update);
ASS_Track *ass_new_track(ASS_Library *library);
void ass_free_track(ASS_Track *track);
void ass_process_codec_private(ASS_Track *track, const char *data, int size);
void ass_process_chunk(ASS_Track *track, const char *data, int size,
      long long timecode, long long duration);
void ass_flush_events(ASS_Track *track);
ASS_Image *ass_render_frame(ASS_Renderer *priv, ASS_Track *track,
      long long now, int *detect_change);

#endif

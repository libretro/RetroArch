#ifndef TESS_GET_TEXT
#define TESS_GET_TEXT

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct
{
   unsigned width;
   unsigned height;
   unsigned bytes_per_pixel;
   void*    data;
} tess_image;

/* if running in RetroArch language should be "eng" or "jpn" */
bool  tess_init(const char* lang_data_dir, const char* language);
void  tess_deinit(void);
char* tess_get_text(tess_image image);

extern char tess_last_error[];

RETRO_END_DECLS

#endif

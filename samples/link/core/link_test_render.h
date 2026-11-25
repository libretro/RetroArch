/* The link test core's two-part picture: what link_test_core.c and each
 * file that draws the picture share */

#ifndef LINK_TEST_RENDER_H
#define LINK_TEST_RENDER_H

#include <libretro.h>

#define LT_RENDER_CELLS 24 /* in a part: three bands of eight stripes */

/* The OpenGL picture's options as they are when not given, which is where
 * they must stay while another picture is drawn */
#define LT_GL_CONTEXT_DEFAULT "core33"
#define LT_GL_ORIGIN_DEFAULT  "top"
#define LT_GL_STATE_DEFAULT   "clean"
#define LT_GL_ASK_DEFAULT     "load"

struct lt_render_host
{
   retro_environment_t environment;
   /* an error line and the error byte */
   void (*fail)(const char *what, uint64_t a, uint64_t b);
   const char *(*option)(const char *key);
   /* the error line of a value an option does not know, and the error byte */
   void (*fail_option)(const char *key, const char *value);
};

struct lt_render
{
   /* in retro_load_game; false: refused */
   bool (*load)(const struct lt_render_host *host);
   /* a frame's start: the size told if it must be, the picture black */
   void (*begin)(unsigned width, unsigned height);
   /* part 0 or 1, handed to the driver before it returns */
   void (*part)(unsigned part, uint64_t value);
   void (*present)(retro_video_refresh_t video, bool dupe);
   /* in retro_unload_game, also when load refused or never ran */
   void (*unload)(void);
};

struct lt_render_cell
{
   unsigned x, y, width, height; /* y from the picture's top row */
   uint32_t rgb;                 /* 0xRRGGBB */
};

/* Cell i of a part drawn with value, in a picture of width x height */
void lt_render_cell(unsigned width, unsigned height, unsigned part, uint64_t value,
      unsigned i, struct lt_render_cell *cell);

/* Tells the frontend a size other than the last one told, where
 * link_test_grow says to tell; false: the frontend refused it */
bool lt_render_tell(unsigned width, unsigned height);

extern const struct lt_render lt_render_soft; /* link_test_core.c */
extern const struct lt_render lt_render_gl;   /* link_test_gl.c */

#endif

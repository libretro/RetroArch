/* Minimal stand-ins for the layers below font_driver.c. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include "gfx/font_driver.h"
#include "gfx/video_driver.h"

extern int read_should_fail;

bool path_is_valid(const char *path) { (void)path; return true; }

bool filestream_read_file(const char *path, void **buf, int64_t *len)
{
   (void)path;
   if (read_should_fail)
   {
      *buf = NULL;
      if (len) *len = 0;
      return false;
   }
   /* Not a real TTF: the renderer must reject it, and whoever read it
    * must release it. */
   *buf = calloc(1, 64);
   if (len) *len = 64;
   return *buf != NULL;
}

static video_driver_state_t vst;
video_driver_state_t *video_state_get_ptr(void) { return &vst; }

/* The lifecycle test drives a stub backend and never links the real
 * stb renderer, so font_driver.c's reference to it needs satisfying.
 * The create test links stb.c itself and defines it for real. */
#ifndef FONT_TEST_REAL_STB
font_renderer_driver_t stb_font_renderer;
#endif

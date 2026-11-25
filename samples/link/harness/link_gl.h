/* The link harness's OpenGL consoles: one surfaceless EGL context and one
 * framebuffer for each. Built only with HAVE_EGL. */

#ifndef LINK_GL_H
#define LINK_GL_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>

typedef struct link_gl link_gl_t;   /* one console's context and framebuffer */

/* SET_HW_RENDER: a context of hw's type, version and profile, current on no
 * thread. NULL: refused, the reason on stderr after
 * "link_harness: console <index>: " */
link_gl_t *link_gl_new(const struct retro_hw_render_callback *hw, unsigned index);
/* After retro_load_game and before context_reset: makes g current and
 * builds its framebuffer. false: the reason on stderr. */
bool link_gl_start(link_gl_t *g, unsigned max_width, unsigned max_height);
/* Before g's console runs: makes g current on this thread unless it is the
 * last that was, and counts the change. false: the reason on stderr the
 * first time, and the context that was current still is. */
bool link_gl_enter(link_gl_t *g);
unsigned long link_gl_switches(void);   /* the changes counted, starts included */
/* A framebuffer no smaller than this, and never smaller than it was. g is
 * current; the core's bindings are put back, and the GL errors it left are
 * dropped. false: the reason on stderr, and no frame larger than it was is
 * read. */
bool link_gl_grow(link_gl_t *g, unsigned max_width, unsigned max_height);
void link_gl_size(const link_gl_t *g, unsigned *width, unsigned *height);
uintptr_t link_gl_framebuffer(const link_gl_t *g);
retro_proc_address_t RETRO_CALLCONV link_gl_proc(const char *sym);
/* The framebuffer's lower-left width x height into argb as 0x00RRGGBB, the
 * picture's top row first. g is current; the core's bindings and pixel-pack
 * state are put back, and the GL errors it left are dropped. false: the
 * read failed, the reason on stderr the first time, and argb is not a
 * picture. */
bool link_gl_read(link_gl_t *g, uint32_t *argb, unsigned width, unsigned height);
/* "gl <GL_VERSION> | <GL_RENDERER>" and a newline, once g is started */
void link_gl_describe(const link_gl_t *g, char *out, size_t len);
const char *link_gl_renderer(const link_gl_t *g);
/* After context_destroy and retro_unload_game. NULL: nothing. */
void link_gl_free(link_gl_t *g);

#endif

#ifndef OVERLAY_TEXTURES_STUBS_H
#define OVERLAY_TEXTURES_STUBS_H

#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>

#define STUB_MAX_TEXTURES 32

struct stub_texture
{
   uint32_t checksum;
   unsigned width;
   unsigned height;
   bool     live;
};

extern struct stub_texture stub_tex[STUB_MAX_TEXTURES];
extern unsigned stub_tex_loads;      /* uploads since stub_reset()   */
extern unsigned stub_tex_live;       /* textures not yet unloaded    */
extern bool     stub_tex_load_fails; /* the driver refuses an upload */

uint32_t stub_checksum(const struct texture_image *img);
void stub_reset(void);
const struct stub_texture *stub_texture_get(uintptr_t id);

#ifdef HAVE_THREADS
extern bool stub_thread_active;    /* the wrapper is there            */
extern bool stub_thread_wins_race; /* it uploads before the poll      */
/* The video thread gets round to its queue; returns how many nodes. */
unsigned stub_video_thread_run(void);
#endif

#endif

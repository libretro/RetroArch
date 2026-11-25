/* One console of the link harness: its own copy of a core, and the
 * callbacks every console shares, which find their console through the
 * scheduler */

#ifndef LINK_CONSOLE_H
#define LINK_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>
#include <dynamic/dylib.h>
#include <encodings/xxh64.h>
#include <link/link_bus.h>
#include <link/link_sched.h>

#include "link_scenario.h"

struct link_gl;

struct link_console_api
{
   void (*set_environment)(retro_environment_t);
   void (*set_video_refresh)(retro_video_refresh_t);
   void (*set_audio_sample)(retro_audio_sample_t);
   void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*set_input_poll)(retro_input_poll_t);
   void (*set_input_state)(retro_input_state_t);
   void (*init)(void);
   void (*deinit)(void);
   void (*get_system_info)(struct retro_system_info *);
   void (*run)(void);
   size_t (*serialize_size)(void);
   bool (*serialize)(void *, size_t);
   bool (*load_game)(const struct retro_game_info *);
   void (*unload_game)(void);
   void *(*get_memory_data)(unsigned);
   size_t (*get_memory_size)(unsigned);
   bool (*unserialize)(const void *, size_t);
   void (*reset)(void);
   void (*get_system_av_info)(struct retro_system_av_info *);
};

struct link_console_variable     /* declared with SET_VARIABLES */
{
   char *key, *fallback;            /* fallback: the first value */
};

struct link_console_picture      /* ARGB, kept for a checkpoint */
{
   uint32_t *argb;
   size_t size;                     /* allocated, in pixels */
   unsigned width, height;
};

struct link_console_finished     /* a frame the console finished in this step */
{
   uint64_t video, audio, state;
   unsigned long frame;             /* its own index */
   bool saved;                      /* state hashes its retro_serialize output */
};

typedef struct link_console
{
   struct link_console_api api;
   xxh64_state_t audio;             /* hash of this frame's audio */
   int16_t *kept_audio;             /* this frame's audio so far, kept while its core
                                       may be saved inside an advance */
   size_t kept_count, kept_cap;
   dylib_t lib;
   char *dir, *save_dir, *core_copy;
   void *rom_data;
   unsigned char *state;            /* retro_serialize output, for the trace */
   size_t state_size;               /* allocated */
   struct link_console_finished *finished;
   size_t finished_count, finished_cap;
   struct link_console_picture shown;   /* its last finished frame's */
   struct link_console_picture drawn;   /* the frame it is in */
   struct link_console_variable *variables;
   size_t variable_count;
   struct retro_memory_descriptor *descriptors;  /* a copy of SET_MEMORY_MAPS's */
   unsigned descriptor_count;
   struct link_gl *gl;              /* the OpenGL context it asked for */
   struct retro_hw_render_callback hw;  /* a copy of what it asked with */
   uint64_t video;                  /* the latest picture */
   enum retro_pixel_format format;
   unsigned long frame;             /* its own frames finished, the index of the one it is in */
   unsigned index, bytes_per_pixel, errors;
   bool started, loaded;
   bool gl_started;                 /* its context is current and was reset */
   bool oversize;                   /* said once: a frame larger than its framebuffer */
   bool resuming;                   /* a load left it inside its frame, which its next run
                                       finishes */
   bool ran;                        /* finished a frame since it started, loaded or was held */
   bool keep;                       /* keeps this frame's picture */
   bool drew, duped;                /* in this frame; drew: kept its picture in drawn */
   bool drew_before;                /* its last finished frame drew */
   bool pictured;                   /* shown is its last frame's, or over a dupe the one before */
} link_console_t;

/* The consoles the callbacks serve. require_renderer: text the renderer of
 * each console with a GPU context must name, or NULL */
void link_console_session(link_sched_t *sched, link_bus_t *bus, link_console_t *consoles,
      const struct link_scenario *scn, const char *system_dir, const char *require_renderer);
/* Before a step's run, with console 0 at its edge: the steps to the next
 * checkpoint (0: none) and that checkpoint's instant as ticks of rate (rate
 * 0: not known), by which each frame keeps its picture or not */
void link_console_step(unsigned long steps, uint64_t at, uint64_t rate);
/* link_sched_done_t: the frame's hashes into finished, its picture into
 * shown, and the console on to its next frame */
void link_console_done(void *user);

/* Copies the core into out/c<n>/ and loads the copy */
bool link_console_open(link_console_t *c, unsigned index, const char *out);
/* retro_init and retro_load_game, the context the core asked for and its
 * context_reset, then save RAM from a copy of the save. A console with a
 * context names its driver in out/c<n>/gpu.txt. */
bool link_console_start(link_console_t *c);
/* link_sched_run_t: one retro_run */
void link_console_run(void *user);
uint64_t link_console_audio(link_console_t *c);
bool link_console_state(link_console_t *c, uint64_t *hash);
/* retro_serialize_size, and retro_serialize into size bytes at data */
size_t link_console_state_size(link_console_t *c);
bool link_console_serialize(link_console_t *c, void *data, size_t size);
bool link_console_unserialize(link_console_t *c, const void *data, size_t len);
/* A load left the console inside its frame: count samples of that frame's
 * audio before the save, little-endian, which its hash starts from */
bool link_console_resume(link_console_t *c, const unsigned char *audio, size_t count);
/* A hold applied: no picture is seen until a frame after it, which may be a
 * dupe of the frame before the hold */
void link_console_held(link_console_t *c);
void link_console_reset(link_console_t *c);
/* A byte of a RETRO_MEMORY_* region; false when the core has no such byte */
bool link_console_byte(link_console_t *c, unsigned memory, unsigned long offset,
      unsigned *value);
/* The byte at a guest address through the core's memory map; false when no
 * descriptor claims the address or the one that does has no memory */
bool link_console_address(const link_console_t *c, size_t address, unsigned *value);
bool link_console_declared(const link_console_t *c, const char *key);
void link_console_close(link_console_t *c);

#endif

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
};

struct link_console_variable     /* declared with SET_VARIABLES */
{
   char *key, *fallback;            /* fallback: the first value */
};

typedef struct link_console
{
   struct link_console_api api;
   xxh64_state_t audio;             /* hash of this frame's audio */
   dylib_t lib;
   char *dir, *save_dir, *core_copy;
   void *rom_data;
   unsigned char *state;            /* retro_serialize output, for the trace */
   size_t state_size;               /* allocated */
   uint32_t *picture;               /* ARGB, kept in a frame before a checkpoint */
   size_t picture_size;
   struct link_console_variable *variables;
   size_t variable_count;
   uint64_t video;                  /* the latest picture */
   enum retro_pixel_format format;
   unsigned index, bytes_per_pixel, errors;
   unsigned picture_width, picture_height;
   bool started, loaded, ran, pictured; /* ran, pictured: in this frame */
} link_console_t;

/* The consoles the callbacks serve, and the frame they are in; with keep,
 * each console's picture of that frame is kept for a checkpoint */
void link_console_session(link_sched_t *sched, link_bus_t *bus, link_console_t *consoles,
      const struct link_scenario *scn, const char *system_dir);
void link_console_frame(unsigned long frame, bool keep);

/* Copies the core into out/c<n>/ and loads the copy */
bool link_console_open(link_console_t *c, unsigned index, const char *out);
/* retro_init and retro_load_game, then save RAM from a copy of the save */
bool link_console_start(link_console_t *c);
/* link_sched_run_t: one retro_run */
void link_console_run(void *user);
uint64_t link_console_audio(link_console_t *c);
bool link_console_state(link_console_t *c, uint64_t *hash);
/* retro_serialize_size, and retro_serialize into size bytes at data */
size_t link_console_state_size(link_console_t *c);
bool link_console_serialize(link_console_t *c, void *data, size_t size);
bool link_console_unserialize(link_console_t *c, const void *data, size_t len);
void link_console_reset(link_console_t *c);
/* A byte of a RETRO_MEMORY_* region; false when the core has no such byte */
bool link_console_byte(link_console_t *c, unsigned memory, unsigned long offset,
      unsigned *value);
bool link_console_declared(const link_console_t *c, const char *key);
void link_console_close(link_console_t *c);

#endif

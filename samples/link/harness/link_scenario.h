/* Scenario files and input scripts of the link harness */

#ifndef LINK_SCENARIO_H
#define LINK_SCENARIO_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <link/link_bus.h>

#define LINK_SCENARIO_COUNTERS 8 /* link_bus_counts members other than blocks */

enum link_scenario_kind
{
   LINK_SCENARIO_CABLE = 0,
   LINK_SCENARIO_UNCABLE,
   LINK_SCENARIO_HOLD,
   LINK_SCENARIO_UNHOLD,
   LINK_SCENARIO_RESET,
   LINK_SCENARIO_SAVE,
   LINK_SCENARIO_CHECKPOINT,
   LINK_SCENARIO_EXPECT,
   LINK_SCENARIO_END
};

struct link_scenario_event       /* one frame line */
{
   char *name;                     /* save, checkpoint */
   unsigned long frame;
   unsigned long offset;           /* expect */
   size_t address;                 /* expect addr */
   unsigned line;
   enum link_scenario_kind kind;
   unsigned console, port;         /* cable: the first end */
   unsigned console_b, port_b;     /* cable: the second end */
   unsigned memory;                /* expect: RETRO_MEMORY_* */
   unsigned value;                 /* expect: the byte */
   bool by_address;                /* expect addr: through the core's memory map */
};

struct link_scenario_change      /* an input script line */
{
   unsigned long frame;
   unsigned mask;
};

struct link_scenario_input
{
   struct link_scenario_change *changes;
   size_t count;
   unsigned console, port, line;
};

struct link_scenario_option
{
   char *key, *value;
   unsigned console, line;
};

struct link_scenario_console
{
   char *core, *rom, *save;        /* resolved paths; rom and save may be NULL */
   size_t stack;                   /* 0: the scheduler's default */
   unsigned line;                  /* 0 until its console line */
};

struct link_scenario_count       /* an expect-count line */
{
   uint64_t value;
   unsigned counter, line;
};

struct link_scenario
{
   char *path, *dir;
   char *system;                   /* resolved, or NULL for the default */
   struct link_scenario_console *consoles;
   struct link_scenario_option *options;
   struct link_scenario_input *inputs;
   struct link_scenario_event *events;
   struct link_scenario_count *counts;
   size_t console_count, option_count, input_count, event_count, count_count;
   uint64_t hash;                  /* xxh64 of the scenario file's bytes */
   int64_t calendar_sec;
   int32_t utc_offset;
   unsigned long end_frame, stuck_frame;
   unsigned stuck_line;            /* 0: no expect-stuck line */
};

struct link_binding              /* --path NAME=FILE */
{
   const char *name, *path;
   size_t name_len;
};

/* Parses a scenario and every input script it names, and resolves every
 * name to a file that exists. On failure prints file:line: and why, and
 * returns false with nothing left to free. */
bool link_scenario_load(struct link_scenario *s, const char *path,
      const struct link_binding *bindings, size_t binding_count);
void link_scenario_free(struct link_scenario *s);

/* The joypad mask a script holds at a frame */
unsigned link_scenario_mask(const struct link_scenario_input *in, unsigned long frame);

const char *link_scenario_counter_name(unsigned counter);
uint64_t link_scenario_counter_value(const struct link_bus_counts *k, unsigned counter);

/* A decimal number with no sign, at most limit */
bool link_scenario_number(const char *s, uint64_t limit, uint64_t *out);

/* dir/name, allocated */
char *link_scenario_join(const char *dir, const char *name);

#endif

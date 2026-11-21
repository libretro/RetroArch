/* link_harness: runs a scenario's consoles on one link bus, frame by
 * frame, writes a trace of per-frame hashes and compares it with an
 * expected one. Exit 0: the run passed; 1: it failed; 2: a usage or input
 * error, with nothing run. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <formats/rpng.h>
#include <streams/file_stream.h>
#include <encodings/xxh64.h>
#include <link/link_bus.h>
#include <link/link_sched.h>

#include "link_scenario.h"
#include "link_console.h"

#define LINK_HARNESS_PASS  0
#define LINK_HARNESS_FAIL  1
#define LINK_HARNESS_USAGE 2
#define LINK_HARNESS_LINE  192
#define LINK_HARNESS_CHUNK 65536
#define LINK_HARNESS_STATE_MAGIC   "LNKS"
#define LINK_HARNESS_STATE_VERSION 1

#define LINK_HARNESS_HI(x) ((unsigned long)((x) >> 32))
#define LINK_HARNESS_LO(x) ((unsigned long)((x) & 0xFFFFFFFFUL))

struct link_harness_input   /* a core, ROM or save file, which must not change */
{
   const char *path;
   uint64_t hash;
};

struct link_harness_sent    /* a journal entry and its place in the journal */
{
   const struct link_bus_sent *e;
   size_t order;
};

struct link_harness_reader  /* the rest of a state file */
{
   const unsigned char *s;
   size_t left;
};

struct link_harness
{
   struct link_scenario scn;
   struct link_binding *bindings;
   const char *scenario, *out, *from, *expect_path;
   unsigned char *state;     /* the --from state, read whole */
   const unsigned char **saved; /* in it, each console's retro_serialize output */
   const unsigned char *saved_bus; /* and the bus blob */
   size_t *saved_len;
   size_t saved_bus_len;
   char *expect_text;
   char **expect;            /* the expected trace's lines */
   struct link_harness_input *inputs;
   link_console_t *consoles;
   link_bus_t *bus;
   link_sched_t *sched;
   RFILE *trace, *messages;
   struct link_bus_sent *journal;
   struct link_harness_sent *sent;
   char *system_dir;
   size_t binding_count, min_stack;
   size_t expect_count, expect_next;
   size_t input_count, journal_cap;
   size_t first_event;       /* the line after the save line that wrote the state, else 0 */
   unsigned long first_frame, frames; /* first_frame: the state's frame, else 0 */
   enum link_sched_pick pick;
   uint32_t seed;
   bool differed, failed;
};

static const char *const link_harness_kind[] = {
   "cable", "uncable", "hold", "unhold", "reset", "save", "checkpoint", "expect", "end"
};

static int link_harness_usage(const char *why)
{
   fprintf(stderr, "link_harness: %s\n"
         "usage: link_harness --scenario FILE --out DIR [--path NAME=FILE]...\n"
         "         [--pick lowest|reversed|shuffled:SEED] [--from STATE]\n"
         "         [--expect TRACE] [--min-stack BYTES]\n", why);
   return LINK_HARNESS_USAGE;
}

static int link_harness_options(struct link_harness *h, int argc, char **argv)
{
   int i;
   if (!(h->bindings = (struct link_binding*)calloc((size_t)argc, sizeof(*h->bindings))))
      return link_harness_usage("out of memory");
   for (i = 1; i < argc; i++)
   {
      const char *arg   = argv[i];
      const char *value = i + 1 < argc ? argv[i + 1] : NULL;
      uint64_t v;
      if (!value)
         return link_harness_usage(arg);
      i++;
      if (!strcmp(arg, "--scenario"))
         h->scenario = value;
      else if (!strcmp(arg, "--out"))
         h->out = value;
      else if (!strcmp(arg, "--from"))
         h->from = value;
      else if (!strcmp(arg, "--expect"))
         h->expect_path = value;
      else if (!strcmp(arg, "--path"))
      {
         const char *eq = strchr(value, '=');
         if (!eq || eq == value || !eq[1])
            return link_harness_usage("--path takes NAME=FILE");
         h->bindings[h->binding_count].name     = value;
         h->bindings[h->binding_count].name_len = (size_t)(eq - value);
         h->bindings[h->binding_count++].path   = eq + 1;
      }
      else if (!strcmp(arg, "--pick"))
      {
         if (!strcmp(value, "lowest"))
            h->pick = LINK_SCHED_PICK_LOWEST;
         else if (!strcmp(value, "reversed"))
            h->pick = LINK_SCHED_PICK_REVERSED;
         else if (!strncmp(value, "shuffled:", 9)
               && link_scenario_number(value + 9, 0xFFFFFFFFUL, &v))
         {
            h->pick = LINK_SCHED_PICK_SHUFFLED;
            h->seed = (uint32_t)v;
         }
         else
            return link_harness_usage("--pick takes lowest, reversed or shuffled:SEED");
      }
      else if (!strcmp(arg, "--min-stack"))
      {
         if (!link_scenario_number(value, (size_t)-1, &v))
            return link_harness_usage("--min-stack takes a number of bytes");
         h->min_stack = (size_t)v;
      }
      else
         return link_harness_usage(arg);
   }
   if (!h->scenario || !h->out)
      return link_harness_usage("--scenario and --out are needed");
   return LINK_HARNESS_PASS;
}

static bool link_harness_read_expect(struct link_harness *h)
{
   void *buf = NULL;
   char *s;
   size_t n  = 1;
   if (!filestream_read_file(h->expect_path, &buf, NULL))
   {
      fprintf(stderr, "link_harness: missing file %s\n", h->expect_path);
      return false;
   }
   h->expect_text = (char*)buf;
   for (s = h->expect_text; *s; s++)
      n += *s == '\n';
   if (!(h->expect = (char**)calloc(n, sizeof(*h->expect))))
      return false;
   for (s = h->expect_text; *s;)
   {
      char *end = strchr(s, '\n');
      size_t len;
      if (end)
         *end = '\0';
      len = strlen(s);
      if (len && s[len - 1] == '\r')
         s[len - 1] = '\0';
      h->expect[h->expect_count++] = s;
      if (!end)
         break;
      s = end + 1;
   }
   return true;
}

static bool link_harness_get(struct link_harness_reader *r, unsigned bytes, uint64_t *v)
{
   unsigned i;
   if (r->left < bytes)
      return false;
   *v = 0;
   for (i = 0; i < bytes; i++)
      *v |= (uint64_t)r->s[i] << (i * 8);
   r->s    += bytes;
   r->left -= bytes;
   return true;
}

/* A size, then that many bytes */
static bool link_harness_get_block(struct link_harness_reader *r, const unsigned char **out,
      size_t *len)
{
   uint64_t v;
   if (!link_harness_get(r, 8, &v) || v > r->left)
      return false;
   *out     = r->s;
   *len     = (size_t)v;
   r->s    += *len;
   r->left -= *len;
   return true;
}

static bool link_harness_bad_state(const struct link_harness *h, const char *why)
{
   fprintf(stderr, "link_harness: %s: bad state: %s\n", h->from, why);
   return false;
}

/* The --from state, read whole and checked against the scenario before
 * anything runs */
static bool link_harness_read_state(struct link_harness *h)
{
   struct link_harness_reader r;
   void *buf   = NULL;
   int64_t len = 0;
   uint64_t v;
   unsigned i;
   if (!filestream_read_file(h->from, &buf, &len))
   {
      fprintf(stderr, "link_harness: missing file %s\n", h->from);
      return false;
   }
   h->state = (unsigned char*)buf;
   r.s      = h->state;
   r.left   = (size_t)len;
   if (r.left < 4 || memcmp(r.s, LINK_HARNESS_STATE_MAGIC, 4))
      return link_harness_bad_state(h, "not a session state");
   r.s    += 4;
   r.left -= 4;
   if (!link_harness_get(&r, 4, &v))
      return link_harness_bad_state(h, "truncated");
   if (v != LINK_HARNESS_STATE_VERSION)
      return link_harness_bad_state(h, "not version 1");
   if (!link_harness_get(&r, 8, &v))
      return link_harness_bad_state(h, "truncated");
   if (v > h->scn.end_frame)
      return link_harness_bad_state(h, "a frame past the scenario's end");
   h->first_frame = (unsigned long)v;
   if (!link_harness_get(&r, 8, &v))
      return link_harness_bad_state(h, "truncated");
   if (v != h->scn.hash)
      return link_harness_bad_state(h, "a state of another scenario");
   for (h->first_event = 0; h->first_event < h->scn.event_count; h->first_event++)
      if (     h->scn.events[h->first_event].frame == h->first_frame
            && h->scn.events[h->first_event].kind == LINK_SCENARIO_SAVE)
         break;
   if (h->first_event == h->scn.event_count)
      return link_harness_bad_state(h, "no save line of the scenario at its frame");
   h->first_event++;
   if (!link_harness_get(&r, 4, &v))
      return link_harness_bad_state(h, "truncated");
   if (v != h->scn.console_count)
      return link_harness_bad_state(h, "a state of another number of consoles");
   if (     !(h->saved = (const unsigned char**)calloc(h->scn.console_count, sizeof(*h->saved)))
         || !(h->saved_len = (size_t*)calloc(h->scn.console_count, sizeof(*h->saved_len))))
      return link_harness_bad_state(h, "out of memory");
   for (i = 0; i < h->scn.console_count; i++)
      if (!link_harness_get_block(&r, &h->saved[i], &h->saved_len[i]))
         return link_harness_bad_state(h, "truncated");
   if (!link_harness_get_block(&r, &h->saved_bus, &h->saved_bus_len))
      return link_harness_bad_state(h, "truncated");
   if (r.left)
      return link_harness_bad_state(h, "bytes past its end");
   return true;
}

static bool link_harness_hash_file(const char *path, uint64_t *out)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   unsigned char *buf;
   xxh64_state_t state;
   int64_t got;
   if (!f)
      return false;
   if (!(buf = (unsigned char*)malloc(LINK_HARNESS_CHUNK)))
   {
      filestream_close(f);
      return false;
   }
   xxh64_reset(&state, 0);
   while ((got = filestream_read(f, buf, LINK_HARNESS_CHUNK)) > 0)
      xxh64_update(&state, buf, (size_t)got);
   free(buf);
   filestream_close(f);
   *out = xxh64_digest(&state);
   return got == 0;
}

/* Every core, ROM and save file, and the --from state, hashed before the run */
static bool link_harness_hash_inputs(struct link_harness *h)
{
   size_t i;
   if (!(h->inputs = (struct link_harness_input*)calloc(h->scn.console_count * 3 + 1,
               sizeof(*h->inputs))))
      return false;
   if (h->from)
      h->inputs[h->input_count++].path = h->from;
   for (i = 0; i < h->scn.console_count; i++)
   {
      const struct link_scenario_console *k = &h->scn.consoles[i];
      h->inputs[h->input_count++].path = k->core;
      if (k->rom)
         h->inputs[h->input_count++].path = k->rom;
      if (k->save)
         h->inputs[h->input_count++].path = k->save;
   }
   for (i = 0; i < h->input_count; i++)
      if (!link_harness_hash_file(h->inputs[i].path, &h->inputs[i].hash))
      {
         fprintf(stderr, "link_harness: cannot read %s\n", h->inputs[i].path);
         return false;
      }
   return true;
}

static void link_harness_check_inputs(struct link_harness *h)
{
   size_t i;
   for (i = 0; i < h->input_count; i++)
   {
      uint64_t hash;
      if (link_harness_hash_file(h->inputs[i].path, &hash) && hash == h->inputs[i].hash)
         continue;
      fprintf(stderr, "link_harness: input file changed: %s\n", h->inputs[i].path);
      h->failed = true;
   }
}

static RFILE *link_harness_create(const char *out, const char *name)
{
   char *path = link_scenario_join(out, name);
   RFILE *f   = path ? filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE) : NULL;
   if (!f)
      fprintf(stderr, "link_harness: cannot write %s\n", path ? path : name);
   free(path);
   return f;
}

static void link_harness_write(RFILE *f, char *line)
{
   size_t len = strlen(line);
   line[len]  = '\n';
   filestream_write(f, line, (int64_t)len + 1);
   line[len]  = '\0';
}

static bool link_harness_outputs(struct link_harness *h)
{
   if (!path_mkdir(h->out))
   {
      fprintf(stderr, "link_harness: cannot make %s\n", h->out);
      return false;
   }
   if (h->scn.system)
      h->system_dir = (char*)malloc(strlen(h->scn.system) + 1);
   else
      h->system_dir = link_scenario_join(h->out, "system");
   if (!h->system_dir)
      return false;
   if (h->scn.system)
      strcpy(h->system_dir, h->scn.system);
   else if (!path_mkdir(h->system_dir))
   {
      fprintf(stderr, "link_harness: cannot make %s\n", h->system_dir);
      return false;
   }
   return (h->trace = link_harness_create(h->out, "trace.txt"))
      && (h->messages = link_harness_create(h->out, "messages.log"));
}

/* The bus, the scheduler and every console loaded */
static bool link_harness_session(struct link_harness *h)
{
   unsigned n = (unsigned)h->scn.console_count;
   unsigned i;
   if (     !(h->bus = link_bus_new(n))
         || !(h->sched = link_sched_new(h->bus))
         || !(h->consoles = (link_console_t*)calloc(n, sizeof(*h->consoles))))
      return false;
   link_sched_set_pick(h->sched, h->pick, h->seed);
   link_console_session(h->sched, h->bus, h->consoles, &h->scn, h->system_dir);
   for (i = 0; i < n; i++)
   {
      size_t stack = h->scn.consoles[i].stack ? h->scn.consoles[i].stack : LINK_SCHED_DEFAULT_STACK;
      if (stack < h->min_stack)
         stack = h->min_stack;
      if (link_sched_add(h->sched, link_console_run, &h->consoles[i], stack) != (int)i)
         return false;
   }
   for (i = 0; i < n; i++)
      if (!link_console_open(&h->consoles[i], i, h->out) || !link_console_start(&h->consoles[i]))
         return false;
   return true;
}

/* After every core is loaded: the bus, then each core's state inside its
 * own entry, then a re-join of its ports */
static bool link_harness_load_state(struct link_harness *h)
{
   unsigned i;
   if (!link_bus_load(h->bus, h->saved_bus, h->saved_bus_len))
      return link_harness_bad_state(h, "the bus refused it");
   for (i = 0; i < h->scn.console_count; i++)
   {
      if (!link_console_unserialize(&h->consoles[i], h->saved[i], h->saved_len[i]))
      {
         fprintf(stderr, "link_harness: %s: bad state: console %u's core refused it\n",
               h->from, i);
         return false;
      }
      link_bus_renumbered(h->bus, i);
   }
   return true;
}

/* Every option key must be one its core declared */
static void link_harness_check_options(struct link_harness *h)
{
   size_t i;
   for (i = 0; i < h->scn.option_count; i++)
   {
      const struct link_scenario_option *o = &h->scn.options[i];
      if (link_console_declared(&h->consoles[o->console], o->key))
         continue;
      fprintf(stderr, "link_harness: %s:%u: console %u's core declares no option %s\n",
            h->scn.path, o->line, o->console, o->key);
      h->failed = true;
   }
}

/* v in decimal: C89's printf has no 64-bit format. s holds 21 chars. */
static const char *link_harness_dec(char *s, uint64_t v)
{
   char *d = s + 20;
   *d      = '\0';
   do
   {
      *--d = (char)('0' + (unsigned)(v % 10));
      v   /= 10;
   } while (v);
   return d;
}

static unsigned char *link_harness_put(unsigned char *s, uint64_t v, unsigned bytes)
{
   unsigned i;
   for (i = 0; i < bytes; i++, v >>= 8)
      *s++ = (unsigned char)(v & 0xFF);
   return s;
}

static bool link_harness_write_file(const char *dir, const char *name, const void *data,
      size_t len)
{
   char *path = link_scenario_join(dir, name);
   bool ok    = path && filestream_write_file(path, data, (int64_t)len);
   if (!ok)
      fprintf(stderr, "link_harness: cannot write %s in %s\n", name, dir);
   free(path);
   return ok;
}

/* out/<name>.state: magic, version, frame, scenario hash, console count,
 * each console's retro_serialize output and the bus blob after its size;
 * little-endian. Each core and the bus write in place. */
static void link_harness_save(struct link_harness *h, const struct link_scenario_event *e,
      unsigned long frame)
{
   size_t bus_len     = link_bus_save_size(h->bus);
   size_t len         = 4 + 4 + 8 + 8 + 4 + 8 + bus_len;
   size_t *sizes      = (size_t*)calloc(h->scn.console_count, sizeof(*sizes));
   unsigned char *buf = NULL, *s;
   char *name         = NULL;
   unsigned i;
   bool ok            = sizes != NULL;
   for (i = 0; ok && i < h->scn.console_count; i++)
   {
      sizes[i] = link_console_state_size(&h->consoles[i]);
      len     += 8 + sizes[i];
   }
   if (ok)
   {
      buf  = (unsigned char*)malloc(len);
      name = (char*)malloc(strlen(e->name) + sizeof(".state"));
      ok   = buf && name;
   }
   if (ok)
   {
      s = buf;
      memcpy(s, LINK_HARNESS_STATE_MAGIC, 4);
      s = link_harness_put(s + 4, LINK_HARNESS_STATE_VERSION, 4);
      s = link_harness_put(s, frame, 8);
      s = link_harness_put(s, h->scn.hash, 8);
      s = link_harness_put(s, h->scn.console_count, 4);
      for (i = 0; ok && i < h->scn.console_count; i++)
      {
         s = link_harness_put(s, sizes[i], 8);
         if (!link_console_serialize(&h->consoles[i], s, sizes[i]))
         {
            fprintf(stderr, "link_harness: %s:%u: console %u: retro_serialize failed\n",
                  h->scn.path, e->line, i);
            ok = false;
         }
         s += sizes[i];
      }
      if (ok)
      {
         s = link_harness_put(s, bus_len, 8);
         sprintf(name, "%s.state", e->name);
         ok = link_bus_save(h->bus, s, bus_len)
            && link_harness_write_file(h->out, name, buf, len);
      }
   }
   if (!ok)
   {
      fprintf(stderr, "link_harness: %s:%u: no state saved\n", h->scn.path, e->line);
      h->failed = true;
   }
   free(name);
   free(buf);
   free(sizes);
}

/* For a person: a PNG of each console's picture in the frame before, the
 * bus dump and the counts. A console that ran the frame before and kept no
 * picture fails the run. */
static void link_harness_checkpoint(struct link_harness *h, const struct link_scenario_event *e)
{
   struct link_bus_counts counts;
   char line[LINK_HARNESS_LINE];
   char dec[21];
   char *dir  = link_scenario_join(h->out, e->name);
   char *text = NULL;
   RFILE *f   = NULL;
   size_t len = 0;
   unsigned i;
   bool ok    = dir && path_mkdir(dir);
   for (i = 0; ok && i < h->scn.console_count; i++)
   {
      const link_console_t *c = &h->consoles[i];
      char *path;
      if (!c->pictured)
      {
         if (c->ran)
         {
            fprintf(stderr, "link_harness: %s:%u: checkpoint %s: console %u kept no picture\n",
                  h->scn.path, e->line, e->name, i);
            h->failed = true;
         }
         continue;
      }
      sprintf(line, "c%u.png", i);
      if (     !(path = link_scenario_join(dir, line))
            || !rpng_save_image_argb(path, c->picture, c->picture_width, c->picture_height,
               c->picture_width * (unsigned)sizeof(*c->picture)))
         ok = false;
      free(path);
   }
   if (ok)
   {
      len = link_bus_dump(h->bus, NULL, 0);
      ok  = (text = (char*)malloc(len + 1)) != NULL;
   }
   if (ok)
   {
      link_bus_dump(h->bus, text, len + 1);
      ok = link_harness_write_file(dir, "bus.txt", text, len);
   }
   if (ok)
      ok = (f = link_harness_create(dir, "counts.txt")) != NULL;
   if (ok)
   {
      link_bus_get_counts(h->bus, &counts);
      for (i = 0; i < LINK_SCENARIO_COUNTERS; i++)
      {
         sprintf(line, "%s %s", link_scenario_counter_name(i),
               link_harness_dec(dec, link_scenario_counter_value(&counts, i)));
         link_harness_write(f, line);
      }
      filestream_close(f);
   }
   if (!ok)
   {
      fprintf(stderr, "link_harness: %s:%u: checkpoint %s not written\n", h->scn.path,
            e->line, e->name);
      h->failed = true;
   }
   free(text);
   free(dir);
}

static void link_harness_expect(struct link_harness *h, const struct link_scenario_event *e)
{
   unsigned got;
   if (!link_console_byte(&h->consoles[e->console], e->memory, e->offset, &got))
   {
      fprintf(stderr, "link_harness: %s:%u: console %u's core has no byte %lu there\n",
            h->scn.path, e->line, e->console, e->offset);
      h->failed = true;
   }
   else if (got != e->value)
   {
      fprintf(stderr, "link_harness: %s:%u: expect failed: console %u byte %lu is %u, not %u\n",
            h->scn.path, e->line, e->console, e->offset, got, e->value);
      h->failed = true;
   }
}

/* A membership line the bus refuses fails the run */
static void link_harness_member(struct link_harness *h, const struct link_scenario_event *e,
      bool done)
{
   if (done)
      return;
   fprintf(stderr, "link_harness: %s:%u: the bus refused the %s\n", h->scn.path, e->line,
         link_harness_kind[e->kind]);
   h->failed = true;
}

/* The scenario's lines at a frame, at the edge before it runs */
static void link_harness_edge(struct link_harness *h, unsigned long frame, size_t *next)
{
   while (*next < h->scn.event_count && h->scn.events[*next].frame == frame)
   {
      const struct link_scenario_event *e = &h->scn.events[(*next)++];
      switch (e->kind)
      {
         case LINK_SCENARIO_CABLE:
            link_harness_member(h, e,
                  link_bus_cable(h->bus, e->console, e->port, e->console_b, e->port_b));
            break;
         case LINK_SCENARIO_UNCABLE:
            link_harness_member(h, e, link_bus_uncable(h->bus, e->console, e->port));
            break;
         case LINK_SCENARIO_HOLD:
            link_harness_member(h, e, link_bus_hold(h->bus, e->console));
            break;
         case LINK_SCENARIO_UNHOLD:
            link_harness_member(h, e, link_bus_unhold(h->bus, e->console));
            break;
         case LINK_SCENARIO_RESET:
            link_console_reset(&h->consoles[e->console]);
            link_bus_renumbered(h->bus, e->console);
            break;
         case LINK_SCENARIO_SAVE:
            link_harness_save(h, e, frame);
            break;
         case LINK_SCENARIO_CHECKPOINT:
            link_harness_checkpoint(h, e);
            break;
         case LINK_SCENARIO_EXPECT:
            link_harness_expect(h, e);
            break;
         default:
            break;
      }
   }
}

/* Whether a checkpoint comes at the edge after this frame, so that its
 * pictures are kept while it runs */
static bool link_harness_keep(const struct link_harness *h, size_t next, unsigned long frame)
{
   for (; next < h->scn.event_count && h->scn.events[next].frame == frame + 1; next++)
      if (h->scn.events[next].kind == LINK_SCENARIO_CHECKPOINT)
         return true;
   return false;
}

/* out/stuck.txt: each console parked at the release, and what bounds it,
 * or that it was parked for advancing without progress */
static void link_harness_stuck(struct link_harness *h, unsigned long frame)
{
   char line[LINK_HARNESS_LINE];
   unsigned n = link_sched_stuck_report(h->sched, NULL, 0);
   struct link_sched_stuck *k = (struct link_sched_stuck*)calloc(n + 1, sizeof(*k));
   RFILE *f   = k ? link_harness_create(h->out, "stuck.txt") : NULL;
   unsigned i;
   if (!f)
   {
      h->failed = true;
      free(k);
      return;
   }
   link_sched_stuck_report(h->sched, k, n);
   for (i = 0; i < n; i++)
   {
      if (k[i].port == UINT_MAX)
         sprintf(line, "F %lu C %u no progress local %08lx%08lx request %08lx%08lx"
               " grant %08lx%08lx", frame, k[i].console,
               LINK_HARNESS_HI(k[i].local_tick), LINK_HARNESS_LO(k[i].local_tick),
               LINK_HARNESS_HI(k[i].request_tick), LINK_HARNESS_LO(k[i].request_tick),
               LINK_HARNESS_HI(k[i].bound), LINK_HARNESS_LO(k[i].bound));
      else
         sprintf(line, "F %lu C %u port %u local %08lx%08lx request %08lx%08lx"
               " bound %08lx%08lx by %u:%u%s", frame, k[i].console, k[i].port,
               LINK_HARNESS_HI(k[i].local_tick), LINK_HARNESS_LO(k[i].local_tick),
               LINK_HARNESS_HI(k[i].request_tick), LINK_HARNESS_LO(k[i].request_tick),
               LINK_HARNESS_HI(k[i].bound), LINK_HARNESS_LO(k[i].bound),
               k[i].bound_console, k[i].bound_port, k[i].edge_unknown ? " edge unknown" : "");
      link_harness_write(f, line);
   }
   filestream_close(f);
   free(k);
}

static void link_harness_where(const char *line, char *where)
{
   unsigned long frame = 0;
   unsigned console    = 0;
   if (sscanf(line, "F %lu C %u", &frame, &console) == 2)
      sprintf(where, "frame %lu console %u", frame, console);
   else if (sscanf(line, "F %lu", &frame) == 1)
      sprintf(where, "frame %lu console bus", frame);
   else
      strcpy(where, "the end");
}

/* Compares a trace line with the expected one, up to the first difference */
static void link_harness_compare(struct link_harness *h, const char *line)
{
   const char *want;
   char where[64];
   if (!h->expect_text || h->differed)
      return;
   want = h->expect_next < h->expect_count ? h->expect[h->expect_next++] : NULL;
   if (want && line && !strcmp(want, line))
      return;
   link_harness_where(line ? line : want, where);
   fprintf(stderr, "trace differs at %s: expected %s got %s\n", where,
         want ? want : "(nothing)", line ? line : "(nothing)");
   h->differed = true;
   h->failed   = true;
}

static void link_harness_trace(struct link_harness *h, unsigned long frame)
{
   char line[LINK_HARNESS_LINE];
   unsigned i;
   uint64_t bus;
   for (i = 0; i < h->scn.console_count; i++)
   {
      link_console_t *c = &h->consoles[i];
      uint64_t audio, state = 0;
      if (link_bus_is_held(h->bus, i))
         sprintf(line, "F %lu C %u HELD", frame, i);
      else
      {
         if (!link_console_state(c, &state))
         {
            fprintf(stderr, "link_harness: console %u: retro_serialize failed in frame %lu\n",
                  i, frame);
            h->failed = true;
         }
         audio = link_console_audio(c);
         sprintf(line, "F %lu C %u V %08lx%08lx A %08lx%08lx S %08lx%08lx", frame, i,
               LINK_HARNESS_HI(c->video), LINK_HARNESS_LO(c->video),
               LINK_HARNESS_HI(audio), LINK_HARNESS_LO(audio),
               LINK_HARNESS_HI(state), LINK_HARNESS_LO(state));
      }
      link_harness_write(h->trace, line);
      link_harness_compare(h, line);
   }
   bus = link_bus_hash(h->bus);
   sprintf(line, "F %lu BUS %08lx%08lx", frame, LINK_HARNESS_HI(bus), LINK_HARNESS_LO(bus));
   link_harness_write(h->trace, line);
   link_harness_compare(h, line);
}

/* Receiving port, then sender, then the sender's own order: never the
 * order the consoles happened to run in */
static int link_harness_sent_cmp(const void *a, const void *b)
{
   const struct link_harness_sent *x = (const struct link_harness_sent*)a;
   const struct link_harness_sent *y = (const struct link_harness_sent*)b;
   if (x->e->to_console != y->e->to_console)
      return x->e->to_console < y->e->to_console ? -1 : 1;
   if (x->e->to_port != y->e->to_port)
      return x->e->to_port < y->e->to_port ? -1 : 1;
   if (x->e->from_console != y->e->from_console)
      return x->e->from_console < y->e->from_console ? -1 : 1;
   if (x->e->from_port != y->e->from_port)
      return x->e->from_port < y->e->from_port ? -1 : 1;
   return x->order < y->order ? -1 : x->order > y->order;
}

static void link_harness_messages(struct link_harness *h, unsigned long frame)
{
   char line[LINK_HARNESS_LINE];
   size_t n = link_bus_journal(h->bus, NULL, 0);
   size_t i;
   if (!n)
      return;
   if (n > h->journal_cap)
   {
      struct link_bus_sent *journal = (struct link_bus_sent*)realloc(h->journal,
            n * sizeof(*journal));
      struct link_harness_sent *sent;
      if (journal)
         h->journal = journal;
      if (!journal || !(sent = (struct link_harness_sent*)realloc(h->sent, n * sizeof(*sent))))
      {
         fprintf(stderr, "link_harness: out of memory in frame %lu\n", frame);
         h->failed = true;
         return;
      }
      h->sent        = sent;
      h->journal_cap = n;
   }
   link_bus_journal(h->bus, h->journal, n);
   for (i = 0; i < n; i++)
   {
      h->sent[i].e     = &h->journal[i];
      h->sent[i].order = i;
   }
   qsort(h->sent, n, sizeof(*h->sent), link_harness_sent_cmp);
   for (i = 0; i < n; i++)
   {
      const struct link_bus_sent *e = h->sent[i].e;
      sprintf(line, "F %lu %u:%u -> %u:%u stamp %08lx%08lx landing %08lx%08lx len %lu"
            " head %02x%02x%02x%02x%02x%02x%02x%02x", frame,
            e->from_console, e->from_port, e->to_console, e->to_port,
            LINK_HARNESS_HI(e->stamp), LINK_HARNESS_LO(e->stamp),
            LINK_HARNESS_HI(e->landing), LINK_HARNESS_LO(e->landing),
            (unsigned long)e->len, e->head[0], e->head[1], e->head[2], e->head[3],
            e->head[4], e->head[5], e->head[6], e->head[7]);
      link_harness_write(h->messages, line);
   }
}

/* Every counter must be the scenario's expect-count, else 0, and no core
 * may have logged an error */
static void link_harness_check_counts(struct link_harness *h)
{
   struct link_bus_counts counts;
   char got[21], want[21];
   unsigned i;
   link_bus_get_counts(h->bus, &counts);
   for (i = 0; i < LINK_SCENARIO_COUNTERS; i++)
   {
      uint64_t v      = link_scenario_counter_value(&counts, i);
      uint64_t wanted = 0;
      size_t j;
      for (j = 0; j < h->scn.count_count; j++)
         if (h->scn.counts[j].counter == i)
            wanted = h->scn.counts[j].value;
      if (v == wanted)
         continue;
      fprintf(stderr, "link_harness: the bus counted %s %s times, not %s\n",
            link_scenario_counter_name(i), link_harness_dec(got, v),
            link_harness_dec(want, wanted));
      h->failed = true;
   }
   for (i = 0; i < h->scn.console_count; i++)
      if (h->consoles[i].errors)
      {
         fprintf(stderr, "link_harness: console %u: errors logged: %u\n", i,
               h->consoles[i].errors);
         h->failed = true;
      }
}

/* Runs from the first frame, or the state's: the lines up to the save line
 * that wrote the state are in it already and are skipped, and so are the
 * expected trace's lines before its frame */
static void link_harness_run(struct link_harness *h)
{
   const struct link_scenario *s = &h->scn;
   unsigned long frame, f;
   size_t next  = h->first_event;
   bool stuck   = false;
   link_harness_check_options(h);
   while (h->expect_next < h->expect_count
         && sscanf(h->expect[h->expect_next], "F %lu", &f) == 1 && f < h->first_frame)
      h->expect_next++;
   for (frame = h->first_frame;; frame++)
   {
      enum link_sched_frame result;
      link_harness_edge(h, frame, &next);
      if (frame == s->end_frame)
         break;
      link_console_frame(frame, link_harness_keep(h, next, frame));
      result = link_sched_run_frame(h->sched);
      h->frames++;
      link_harness_trace(h, frame);
      link_harness_messages(h, frame);
      if (result == LINK_SCHED_FRAME_RELEASED)
      {
         link_harness_stuck(h, frame);
         stuck = true;
         if (!s->stuck_line || frame != s->stuck_frame)
         {
            if (s->stuck_line)
               fprintf(stderr, "link_harness: %s:%u: the bus was released in frame %lu, not %lu\n",
                     s->path, s->stuck_line, frame, s->stuck_frame);
            else
               fprintf(stderr, "link_harness: the bus was released in frame %lu\n", frame);
            h->failed = true;
         }
         break;
      }
   }
   if (s->stuck_line && !stuck)
   {
      fprintf(stderr, "link_harness: %s:%u: no release in frame %lu\n", s->path,
            s->stuck_line, s->stuck_frame);
      h->failed = true;
   }
   if (h->expect_next < h->expect_count)
      link_harness_compare(h, NULL);
   link_harness_check_counts(h);
}

/* Every core unloaded; closing twice does nothing */
static void link_harness_close(struct link_harness *h)
{
   size_t i;
   if (h->consoles)
      for (i = 0; i < h->scn.console_count; i++)
         link_console_close(&h->consoles[i]);
}

static void link_harness_free(struct link_harness *h)
{
   link_harness_close(h);
   link_sched_free(h->sched);
   link_bus_free(h->bus);
   if (h->trace)
      filestream_close(h->trace);
   if (h->messages)
      filestream_close(h->messages);
   free(h->consoles);
   free(h->state);
   free(h->saved);
   free(h->saved_len);
   free(h->journal);
   free(h->sent);
   free(h->system_dir);
   free(h->inputs);
   free(h->expect);
   free(h->expect_text);
   free(h->bindings);
   link_scenario_free(&h->scn);
   free(h);
}

static int link_harness_main(struct link_harness *h, int argc, char **argv)
{
   int rc;
   if ((rc = link_harness_options(h, argc, argv)) != LINK_HARNESS_PASS)
      return rc;
   if (!link_scenario_load(&h->scn, h->scenario, h->bindings, h->binding_count))
      return LINK_HARNESS_USAGE;
   if (     (h->from && !link_harness_read_state(h))
         || (h->expect_path && !link_harness_read_expect(h))
         || !link_harness_hash_inputs(h)
         || !link_harness_outputs(h))
      return LINK_HARNESS_USAGE;
   if (!link_harness_session(h))
      h->failed = true;
   else if (h->from && !link_harness_load_state(h))
      return LINK_HARNESS_USAGE;
   else
      link_harness_run(h);
   return LINK_HARNESS_PASS;
}

int main(int argc, char **argv)
{
   struct link_harness *h = (struct link_harness*)calloc(1, sizeof(*h));
   int rc;
   if (!h)
      return LINK_HARNESS_USAGE;
   if ((rc = link_harness_main(h, argc, argv)) == LINK_HARNESS_PASS)
   {
      link_harness_close(h);
      link_harness_check_inputs(h);
      if (h->failed)
         rc = LINK_HARNESS_FAIL;
      else
         printf("link_harness: %s passed, %lu frames\n", h->scenario, h->frames);
   }
   link_harness_free(h);
   return rc;
}

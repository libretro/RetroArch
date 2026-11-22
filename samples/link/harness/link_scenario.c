/* Scenario files and input scripts of the link harness */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <encodings/xxh64.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "link_scenario.h"

#define LINK_SCENARIO_FIELDS      8
#define LINK_SCENARIO_MAX_CONSOLE 255
#define LINK_SCENARIO_MAX_PORT    65535
#define LINK_SCENARIO_MAX_FRAME   0xFFFFFFFFUL

struct link_scenario_parse     /* the file being read and where */
{
   struct link_scenario *s;
   const struct link_binding *bindings;
   size_t binding_count;
   const char *file;
   unsigned line;
   bool ended;
};

static const struct
{
   const char *name;
   size_t offset;
} link_scenario_counter[LINK_SCENARIO_COUNTERS] = {
   { "lowered_promise",        offsetof(struct link_bus_counts, lowered_promise) },
   { "low_stamp",              offsetof(struct link_bus_counts, low_stamp) },
   { "request_past_promise",   offsetof(struct link_bus_counts, request_past_promise) },
   { "request_past_frame_end", offsetof(struct link_bus_counts, request_past_frame_end) },
   { "request_past_earliest",  offsetof(struct link_bus_counts, request_past_earliest) },
   { "skipped_publish",        offsetof(struct link_bus_counts, skipped_publish) },
   { "left_out",               offsetof(struct link_bus_counts, left_out) },
   { "misplaced_call",         offsetof(struct link_bus_counts, misplaced_call) }
};

static bool link_scenario_fail(const struct link_scenario_parse *p, const char *what, const char *arg)
{
   if (p->line)
      fprintf(stderr, "link_harness: %s:%u: %s%s%s\n", p->file, p->line, what,
            arg ? ": " : "", arg ? arg : "");
   else
      fprintf(stderr, "link_harness: %s: %s%s%s\n", p->file, what,
            arg ? ": " : "", arg ? arg : "");
   return false;
}

static char *link_scenario_strdup(const char *s)
{
   char *copy = (char*)malloc(strlen(s) + 1);
   if (copy)
      strcpy(copy, s);
   return copy;
}

/* items grown by one zeroed element */
static void *link_scenario_more(void *items, size_t count, size_t size)
{
   unsigned char *more = (unsigned char*)realloc(items, (count + 1) * size);
   if (more)
      memset(more + count * size, 0, size);
   return more;
}

char *link_scenario_join(const char *dir, const char *name)
{
   size_t len = strlen(dir);
   bool slash = len && (dir[len - 1] == '/' || dir[len - 1] == '\\');
   char *path = (char*)malloc(len + strlen(name) + 2);
   if (!path)
      return NULL;
   strcpy(path, dir);
   if (!slash)
      strcat(path, "/");
   strcat(path, name);
   return path;
}

bool link_scenario_number(const char *s, uint64_t limit, uint64_t *out)
{
   uint64_t v = 0;
   if (!s || !*s)
      return false;
   for (; *s; s++)
   {
      unsigned d;
      if (*s < '0' || *s > '9')
         return false;
      d = (unsigned)(*s - '0');
      if (d > limit || v > (limit - d) / 10)
         return false;
      v = v * 10 + d;
   }
   *out = v;
   return true;
}

static bool link_scenario_unsigned(const char *s, unsigned long limit, unsigned long *out)
{
   uint64_t v;
   if (!link_scenario_number(s, limit, &v))
      return false;
   *out = (unsigned long)v;
   return true;
}

static bool link_scenario_signed(const char *s, int64_t *out)
{
   uint64_t top = (uint64_t)1 << 63;
   uint64_t v;
   bool negative = *s == '-';
   if (!link_scenario_number(s + negative, negative ? top : top - 1, &v))
      return false;
   *out = negative ? -(int64_t)(v - 1) - 1 : (int64_t)v;
   return true;
}

static bool link_scenario_hex(const char *s, unsigned long limit, unsigned long *out)
{
   unsigned long v = 0;
   if (!*s)
      return false;
   for (; *s; s++)
   {
      unsigned d;
      if (*s >= '0' && *s <= '9')
         d = (unsigned)(*s - '0');
      else if (*s >= 'a' && *s <= 'f')
         d = (unsigned)(*s - 'a' + 10);
      else if (*s >= 'A' && *s <= 'F')
         d = (unsigned)(*s - 'A' + 10);
      else
         return false;
      if (d > limit || v > (limit - d) / 16)
         return false;
      v = v * 16 + d;
   }
   *out = v;
   return true;
}

/* Up to limit whitespace-separated fields from *text, which is left at what follows */
static unsigned link_scenario_split(char **text, char **field, unsigned limit)
{
   char *s    = *text;
   unsigned n = 0;
   while (*s == ' ' || *s == '\t')
      s++;
   while (*s && n < limit)
   {
      field[n++] = s;
      while (*s && *s != ' ' && *s != '\t')
         s++;
      if (*s)
         *s++ = '\0';
      while (*s == ' ' || *s == '\t')
         s++;
   }
   *text = s;
   return n;
}

/* The next line of *text, comment and trailing blanks cut; NULL at the end */
static char *link_scenario_next_line(char **text)
{
   char *line = *text;
   char *end;
   size_t len;
   if (!line)
      return NULL;
   if ((end = strchr(line, '\n')))
   {
      *end  = '\0';
      *text = end + 1;
   }
   else
      *text = NULL;
   if ((end = strchr(line, '#')))
      *end = '\0';
   len = strlen(line);
   while (len && (line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t'))
      line[--len] = '\0';
   return line;
}

static bool link_scenario_exists(const char *path, bool directory)
{
   if (directory)
      return path_is_directory(path);
   return path_is_valid(path) && !path_is_directory(path);
}

/* A name bound with --path, else the file of that name beside the scenario */
static char *link_scenario_resolve(struct link_scenario_parse *p, const char *name, bool directory)
{
   size_t len = strlen(name);
   size_t i;
   char *path;
   for (i = p->binding_count; i-- > 0;)
   {
      if (p->bindings[i].name_len != len || strncmp(p->bindings[i].name, name, len))
         continue;
      if (!link_scenario_exists(p->bindings[i].path, directory))
      {
         link_scenario_fail(p, "missing file", p->bindings[i].path);
         return NULL;
      }
      if (!(path = link_scenario_strdup(p->bindings[i].path)))
         link_scenario_fail(p, "out of memory", NULL);
      return path;
   }
   if (!(path = link_scenario_join(p->s->dir, name)))
   {
      link_scenario_fail(p, "out of memory", NULL);
      return NULL;
   }
   if (link_scenario_exists(path, directory))
      return path;
   free(path);
   link_scenario_fail(p, "a name with no --path binding and no file beside the scenario", name);
   return NULL;
}

static bool link_scenario_console_number(struct link_scenario_parse *p, const char *s, unsigned *out)
{
   unsigned long v;
   if (!link_scenario_unsigned(s, LINK_SCENARIO_MAX_CONSOLE, &v))
      return link_scenario_fail(p, "not a console number", s);
   *out = (unsigned)v;
   return true;
}

static bool link_scenario_end_point(struct link_scenario_parse *p, char *s,
      unsigned *console, unsigned *port)
{
   char *colon = strchr(s, ':');
   unsigned long v;
   if (!colon)
      return link_scenario_fail(p, "not console:port", s);
   *colon = '\0';
   if (!link_scenario_console_number(p, s, console))
      return false;
   if (!link_scenario_unsigned(colon + 1, LINK_SCENARIO_MAX_PORT, &v))
      return link_scenario_fail(p, "not a port number", colon + 1);
   *port = (unsigned)v;
   return true;
}

/* Output names become file and folder names in the output folder */
static bool link_scenario_output_name(struct link_scenario_parse *p, const char *s, char **out)
{
   const char *c;
   for (c = s; *c; c++)
      if (     !(*c >= 'a' && *c <= 'z') && !(*c >= 'A' && *c <= 'Z')
            && !(*c >= '0' && *c <= '9') && *c != '_' && *c != '-'
            && !(*c == '.' && c != s))
         return link_scenario_fail(p, "a name of letters, digits, _, - and . is needed", s);
   if (!(*out = link_scenario_strdup(s)))
      return link_scenario_fail(p, "out of memory", NULL);
   return true;
}

static bool link_scenario_script(struct link_scenario_parse *outer,
      struct link_scenario_input *in, const char *path)
{
   struct link_scenario_parse p;
   void *buf   = NULL;
   char *text;
   char *line;
   bool ok     = true;
   p           = *outer;
   p.file      = path;
   p.line      = 0;
   if (!filestream_read_file(path, &buf, NULL))
      return link_scenario_fail(outer, "missing file", path);
   text = (char*)buf;
   while (ok && (line = link_scenario_next_line(&text)))
   {
      char *field[3];
      unsigned long frame, mask;
      unsigned n;
      struct link_scenario_change *more;
      p.line++;
      if (!(n = link_scenario_split(&line, field, 3)))
         continue;
      if (n != 2)
         ok = link_scenario_fail(&p, "a line is <frame> <hex joypad mask>", NULL);
      else if (!link_scenario_unsigned(field[0], LINK_SCENARIO_MAX_FRAME, &frame))
         ok = link_scenario_fail(&p, "not a frame number", field[0]);
      else if (!link_scenario_hex(field[1], 0xFFFFUL, &mask))
         ok = link_scenario_fail(&p, "not a hex joypad mask", field[1]);
      else if (in->count && frame < in->changes[in->count - 1].frame)
         ok = link_scenario_fail(&p, "frames out of order", field[0]);
      else if (!(more = (struct link_scenario_change*)link_scenario_more(
                  in->changes, in->count, sizeof(*more))))
         ok = link_scenario_fail(&p, "out of memory", NULL);
      else
      {
         in->changes                   = more;
         in->changes[in->count].frame  = frame;
         in->changes[in->count++].mask = (unsigned)mask;
      }
   }
   free(buf);
   return ok;
}

static bool link_scenario_console_line(struct link_scenario_parse *p, char **arg, unsigned argc)
{
   struct link_scenario *s = p->s;
   struct link_scenario_console *k;
   unsigned n, i;
   if (argc < 2)
      return link_scenario_fail(p, "console <n> core=<name> [rom=<name>] [save=<name>] [stack=<bytes>]", NULL);
   if (!link_scenario_console_number(p, arg[0], &n))
      return false;
   while (s->console_count <= n)
   {
      if (!(k = (struct link_scenario_console*)link_scenario_more(
                  s->consoles, s->console_count, sizeof(*k))))
         return link_scenario_fail(p, "out of memory", NULL);
      s->consoles = k;
      s->console_count++;
   }
   k = &s->consoles[n];
   if (k->line)
      return link_scenario_fail(p, "a second console line for the same console", arg[0]);
   k->line = p->line;
   for (i = 1; i < argc; i++)
   {
      char *value = strchr(arg[i], '=');
      char **slot = NULL;
      if (!value)
         return link_scenario_fail(p, "not key=value", arg[i]);
      *value++ = '\0';
      if (!strcmp(arg[i], "core"))
         slot = &k->core;
      else if (!strcmp(arg[i], "rom"))
         slot = &k->rom;
      else if (!strcmp(arg[i], "save"))
         slot = &k->save;
      else if (!strcmp(arg[i], "stack"))
      {
         uint64_t v;
         if (!link_scenario_number(value, (size_t)-1, &v) || !v)
            return link_scenario_fail(p, "not a stack size", value);
         k->stack = (size_t)v;
         continue;
      }
      else
         return link_scenario_fail(p, "unknown console field", arg[i]);
      if (*slot)
         return link_scenario_fail(p, "a field given twice", arg[i]);
      if (!(*slot = link_scenario_resolve(p, value, false)))
         return false;
   }
   if (!k->core)
      return link_scenario_fail(p, "a console line names no core", NULL);
   return true;
}

static bool link_scenario_option_line(struct link_scenario_parse *p, char *rest)
{
   struct link_scenario *s = p->s;
   struct link_scenario_option *o;
   char *field[2];
   unsigned n;
   if (link_scenario_split(&rest, field, 2) != 2 || !*rest)
      return link_scenario_fail(p, "option <n> <key> <value>", NULL);
   if (!link_scenario_console_number(p, field[0], &n))
      return false;
   if (!(o = (struct link_scenario_option*)link_scenario_more(
               s->options, s->option_count, sizeof(*o))))
      return link_scenario_fail(p, "out of memory", NULL);
   s->options = o;
   o         += s->option_count++;
   o->console = n;
   o->line    = p->line;
   o->key     = link_scenario_strdup(field[1]);
   o->value   = link_scenario_strdup(rest);
   if (!o->key || !o->value)
      return link_scenario_fail(p, "out of memory", NULL);
   return true;
}

static bool link_scenario_input_line(struct link_scenario_parse *p, char **arg, unsigned argc)
{
   struct link_scenario *s = p->s;
   struct link_scenario_input *in;
   unsigned long port;
   unsigned n;
   size_t i;
   char *path;
   bool ok;
   if (argc != 3)
      return link_scenario_fail(p, "input <n> <port> <file>", NULL);
   if (!link_scenario_console_number(p, arg[0], &n))
      return false;
   if (!link_scenario_unsigned(arg[1], LINK_SCENARIO_MAX_PORT, &port))
      return link_scenario_fail(p, "not an input port", arg[1]);
   for (i = 0; i < s->input_count; i++)
      if (s->inputs[i].console == n && s->inputs[i].port == port)
         return link_scenario_fail(p, "a second script for the same input port", arg[1]);
   if (!(in = (struct link_scenario_input*)link_scenario_more(
               s->inputs, s->input_count, sizeof(*in))))
      return link_scenario_fail(p, "out of memory", NULL);
   s->inputs   = in;
   in         += s->input_count++;
   in->console = n;
   in->port    = (unsigned)port;
   in->line    = p->line;
   path        = path_is_absolute(arg[2])
      ? link_scenario_strdup(arg[2]) : link_scenario_join(s->dir, arg[2]);
   if (!path)
      return link_scenario_fail(p, "out of memory", NULL);
   ok = link_scenario_script(p, in, path);
   free(path);
   return ok;
}

static bool link_scenario_header_line(struct link_scenario_parse *p, const char *cmd,
      char **arg, unsigned argc)
{
   struct link_scenario *s = p->s;
   if (!strcmp(cmd, "console"))
      return link_scenario_console_line(p, arg, argc);
   if (!strcmp(cmd, "input"))
      return link_scenario_input_line(p, arg, argc);
   if (!strcmp(cmd, "calendar"))
   {
      int64_t sec, offset;
      if (argc != 2)
         return link_scenario_fail(p, "calendar <sec> <utc_offset>", NULL);
      if (!link_scenario_signed(arg[0], &sec))
         return link_scenario_fail(p, "not a number of seconds", arg[0]);
      if (!link_scenario_signed(arg[1], &offset) || offset < -86400 || offset > 86400)
         return link_scenario_fail(p, "not a UTC offset in seconds", arg[1]);
      s->calendar_sec = sec;
      s->utc_offset   = (int32_t)offset;
      return true;
   }
   if (!strcmp(cmd, "system"))
   {
      if (argc != 1)
         return link_scenario_fail(p, "system <name>", NULL);
      if (s->system)
         return link_scenario_fail(p, "a second system line", NULL);
      return (s->system = link_scenario_resolve(p, arg[0], true)) != NULL;
   }
   if (!strcmp(cmd, "expect-count"))
   {
      struct link_scenario_count *k;
      unsigned i;
      uint64_t value;
      if (argc != 2)
         return link_scenario_fail(p, "expect-count <counter> <value>", NULL);
      for (i = 0; i < LINK_SCENARIO_COUNTERS; i++)
         if (!strcmp(arg[0], link_scenario_counter[i].name))
            break;
      if (i == LINK_SCENARIO_COUNTERS)
         return link_scenario_fail(p, "not a counter", arg[0]);
      for (k = s->counts; k < s->counts + s->count_count; k++)
         if (k->counter == i)
            return link_scenario_fail(p, "a second expect-count line for the same counter", arg[0]);
      if (!link_scenario_number(arg[1], ~(uint64_t)0, &value))
         return link_scenario_fail(p, "not a count", arg[1]);
      if (!(k = (struct link_scenario_count*)link_scenario_more(
                  s->counts, s->count_count, sizeof(*k))))
         return link_scenario_fail(p, "out of memory", NULL);
      s->counts  = k;
      k         += s->count_count++;
      k->counter = i;
      k->value   = value;
      k->line    = p->line;
      return true;
   }
   if (!strcmp(cmd, "expect-stuck"))
   {
      if (argc != 1)
         return link_scenario_fail(p, "expect-stuck <frame>", NULL);
      if (s->stuck_line)
         return link_scenario_fail(p, "a second expect-stuck line", NULL);
      if (!link_scenario_unsigned(arg[0], LINK_SCENARIO_MAX_FRAME, &s->stuck_frame))
         return link_scenario_fail(p, "not a frame number", arg[0]);
      s->stuck_line = p->line;
      return true;
   }
   return link_scenario_fail(p, "unknown command", cmd);
}

static bool link_scenario_frame_line(struct link_scenario_parse *p, unsigned long frame,
      const char *cmd, char **arg, unsigned argc)
{
   struct link_scenario *s = p->s;
   struct link_scenario_event *e;
   if (s->event_count && frame < s->events[s->event_count - 1].frame)
      return link_scenario_fail(p, "frame lines out of order", NULL);
   if (!(e = (struct link_scenario_event*)link_scenario_more(
               s->events, s->event_count, sizeof(*e))))
      return link_scenario_fail(p, "out of memory", NULL);
   s->events = e;
   e        += s->event_count++;
   e->frame  = frame;
   e->line   = p->line;
   if (!strcmp(cmd, "cable"))
   {
      e->kind = LINK_SCENARIO_CABLE;
      if (argc != 2)
         return link_scenario_fail(p, "<frame> cable <c>:<p> <c>:<p>", NULL);
      return link_scenario_end_point(p, arg[0], &e->console, &e->port)
         && link_scenario_end_point(p, arg[1], &e->console_b, &e->port_b);
   }
   if (!strcmp(cmd, "uncable"))
   {
      e->kind = LINK_SCENARIO_UNCABLE;
      if (argc != 1)
         return link_scenario_fail(p, "<frame> uncable <c>:<p>", NULL);
      return link_scenario_end_point(p, arg[0], &e->console, &e->port);
   }
   if (!strcmp(cmd, "hold") || !strcmp(cmd, "unhold") || !strcmp(cmd, "reset"))
   {
      e->kind = !strcmp(cmd, "hold") ? LINK_SCENARIO_HOLD
         : !strcmp(cmd, "unhold") ? LINK_SCENARIO_UNHOLD : LINK_SCENARIO_RESET;
      if (argc != 1)
         return link_scenario_fail(p, "<frame> hold|unhold|reset <c>", NULL);
      return link_scenario_console_number(p, arg[0], &e->console);
   }
   if (!strcmp(cmd, "save") || !strcmp(cmd, "checkpoint"))
   {
      size_t i;
      e->kind = !strcmp(cmd, "save") ? LINK_SCENARIO_SAVE : LINK_SCENARIO_CHECKPOINT;
      if (argc != 1)
         return link_scenario_fail(p, "<frame> save|checkpoint <name>", NULL);
      /* A --from run finds the save line that wrote its state by frame */
      if (e->kind == LINK_SCENARIO_SAVE)
         for (i = s->event_count - 1; i > 0 && s->events[i - 1].frame == frame; i--)
            if (s->events[i - 1].kind == LINK_SCENARIO_SAVE)
               return link_scenario_fail(p, "a second save line at one frame", NULL);
      return link_scenario_output_name(p, arg[0], &e->name);
   }
   if (!strcmp(cmd, "expect"))
   {
      unsigned long value;
      e->kind = LINK_SCENARIO_EXPECT;
      if (argc != 4)
         return link_scenario_fail(p,
               "<frame> expect <c> save|rtc|system|video <offset>|addr <address> <byte>", NULL);
      if (!link_scenario_console_number(p, arg[0], &e->console))
         return false;
      if (!strcmp(arg[1], "save"))
         e->memory = RETRO_MEMORY_SAVE_RAM;
      else if (!strcmp(arg[1], "rtc"))
         e->memory = RETRO_MEMORY_RTC;
      else if (!strcmp(arg[1], "system"))
         e->memory = RETRO_MEMORY_SYSTEM_RAM;
      else if (!strcmp(arg[1], "video"))
         e->memory = RETRO_MEMORY_VIDEO_RAM;
      else if (!strcmp(arg[1], "addr"))
         e->by_address = true;
      else
         return link_scenario_fail(p, "not save, rtc, system, video or addr", arg[1]);
      if (e->by_address)
      {
         uint64_t address;
         if (!link_scenario_number(arg[2], (size_t)-1, &address))
            return link_scenario_fail(p, "not an address", arg[2]);
         e->address = (size_t)address;
      }
      else if (!link_scenario_unsigned(arg[2], 0xFFFFFFFFUL, &e->offset))
         return link_scenario_fail(p, "not an offset", arg[2]);
      if (!link_scenario_unsigned(arg[3], 0xFF, &value))
         return link_scenario_fail(p, "not a byte", arg[3]);
      e->value = (unsigned)value;
      return true;
   }
   if (!strcmp(cmd, "end"))
   {
      e->kind = LINK_SCENARIO_END;
      if (argc)
         return link_scenario_fail(p, "<frame> end", NULL);
      s->end_frame = frame;
      p->ended     = true;
      return true;
   }
   return link_scenario_fail(p, "unknown command", cmd);
}

static bool link_scenario_line(struct link_scenario_parse *p, char *line)
{
   char *field[LINK_SCENARIO_FIELDS];
   unsigned long frame;
   unsigned argc;
   if (!link_scenario_split(&line, field, 1))
      return true;
   if (p->ended)
      return link_scenario_fail(p, "a line after end", NULL);
   if (!strcmp(field[0], "option"))
      return link_scenario_option_line(p, line);
   if (*field[0] < '0' || *field[0] > '9')
   {
      argc = link_scenario_split(&line, field + 1, LINK_SCENARIO_FIELDS - 1);
      if (*line)
         return link_scenario_fail(p, "too many fields", NULL);
      return link_scenario_header_line(p, field[0], field + 1, argc);
   }
   if (!link_scenario_unsigned(field[0], LINK_SCENARIO_MAX_FRAME, &frame))
      return link_scenario_fail(p, "not a frame number", field[0]);
   if (!link_scenario_split(&line, field + 1, 1))
      return link_scenario_fail(p, "a frame number with no command", NULL);
   argc = link_scenario_split(&line, field + 2, LINK_SCENARIO_FIELDS - 2);
   if (*line)
      return link_scenario_fail(p, "too many fields", NULL);
   return link_scenario_frame_line(p, frame, field[1], field + 2, argc);
}

/* Console numbers have no gaps, every line names a console there is, and
 * no line but end comes after an expect-stuck frame */
static bool link_scenario_check(struct link_scenario_parse *p)
{
   struct link_scenario *s = p->s;
   size_t i;
   p->line = 0;
   if (!p->ended)
      return link_scenario_fail(p, "no end line", NULL);
   if (!s->console_count)
      return link_scenario_fail(p, "no console line", NULL);
   for (i = 0; i < s->console_count; i++)
      if (!s->consoles[i].line)
         return link_scenario_fail(p, "consoles are not numbered without gaps", NULL);
   for (i = 0; i < s->option_count; i++)
      if (s->options[i].console >= s->console_count)
      {
         p->line = s->options[i].line;
         return link_scenario_fail(p, "no such console", NULL);
      }
   for (i = 0; i < s->input_count; i++)
      if (s->inputs[i].console >= s->console_count)
      {
         p->line = s->inputs[i].line;
         return link_scenario_fail(p, "no such console", NULL);
      }
   for (i = 0; i < s->event_count; i++)
   {
      const struct link_scenario_event *e = &s->events[i];
      if (s->stuck_line && e->kind != LINK_SCENARIO_END && e->frame > s->stuck_frame)
      {
         p->line = e->line;
         return link_scenario_fail(p, "a line after the expect-stuck frame never acts", NULL);
      }
      if (e->kind == LINK_SCENARIO_END || e->kind == LINK_SCENARIO_SAVE
            || e->kind == LINK_SCENARIO_CHECKPOINT)
         continue;
      if (e->console >= s->console_count
            || (e->kind == LINK_SCENARIO_CABLE && e->console_b >= s->console_count))
      {
         p->line = e->line;
         return link_scenario_fail(p, "no such console", NULL);
      }
   }
   return true;
}

bool link_scenario_load(struct link_scenario *s, const char *path,
      const struct link_binding *bindings, size_t binding_count)
{
   struct link_scenario_parse p;
   void *buf   = NULL;
   int64_t len = 0;
   char *text, *line, *slash;
   bool ok     = true;

   memset(s, 0, sizeof(*s));
   s->calendar_sec  = 1000000000;
   memset(&p, 0, sizeof(p));
   p.s             = s;
   p.bindings      = bindings;
   p.binding_count = binding_count;
   p.file          = path;
   if (!(s->path = link_scenario_strdup(path)) || !(s->dir = link_scenario_strdup(path)))
      ok = link_scenario_fail(&p, "out of memory", NULL);
   else
   {
      slash = strrchr(s->dir, '/');
      if (!slash || (strrchr(s->dir, '\\') && strrchr(s->dir, '\\') > slash))
         slash = strrchr(s->dir, '\\');
      if (slash == s->dir)
         slash[1] = '\0';
      else if (slash)
         *slash = '\0';
      else
         strcpy(s->dir, ".");
      if (!link_scenario_exists(path, false) || !filestream_read_file(path, &buf, &len))
         ok = link_scenario_fail(&p, "missing file", NULL);
      else
         s->hash = xxh64(buf, (size_t)len, 0);
   }
   text = (char*)buf;
   while (ok && (line = link_scenario_next_line(&text)))
   {
      p.line++;
      ok = link_scenario_line(&p, line);
   }
   free(buf);
   if (ok)
      ok = link_scenario_check(&p);
   if (!ok)
      link_scenario_free(s);
   return ok;
}

void link_scenario_free(struct link_scenario *s)
{
   size_t i;
   for (i = 0; i < s->console_count; i++)
   {
      free(s->consoles[i].core);
      free(s->consoles[i].rom);
      free(s->consoles[i].save);
   }
   for (i = 0; i < s->option_count; i++)
   {
      free(s->options[i].key);
      free(s->options[i].value);
   }
   for (i = 0; i < s->input_count; i++)
      free(s->inputs[i].changes);
   for (i = 0; i < s->event_count; i++)
      free(s->events[i].name);
   free(s->consoles);
   free(s->options);
   free(s->inputs);
   free(s->events);
   free(s->counts);
   free(s->system);
   free(s->path);
   free(s->dir);
   memset(s, 0, sizeof(*s));
}

unsigned link_scenario_mask(const struct link_scenario_input *in, unsigned long frame)
{
   unsigned mask = 0;
   size_t i;
   for (i = 0; i < in->count && in->changes[i].frame <= frame; i++)
      mask = in->changes[i].mask;
   return mask;
}

const char *link_scenario_counter_name(unsigned counter)
{
   return counter < LINK_SCENARIO_COUNTERS ? link_scenario_counter[counter].name : NULL;
}

uint64_t link_scenario_counter_value(const struct link_bus_counts *k, unsigned counter)
{
   if (counter >= LINK_SCENARIO_COUNTERS)
      return 0;
   return *(const uint64_t*)((const char*)k + link_scenario_counter[counter].offset);
}

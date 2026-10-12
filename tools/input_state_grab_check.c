/* Input drivers ask the frontend by name; they do not take its state.
 *
 * An input driver, a joypad driver or one of their helpers that needs
 * something from the rest of RetroArch used to take the whole state it
 * was in and pick the field out:
 *
 *     if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
 *     input_state_get_ptr()->flags |= INP_FLAG_NATIVE_KB_SHOWN;
 *     config_get_ptr()->uints.input_mouse_index[port]
 *
 * That ties every driver to the layout of the settings, the run loop,
 * the menu and the input frontend, and nothing says what a driver may
 * read or change. They are being replaced, call by call, with a named
 * question or statement on the side that owns the state:
 *
 *     if (menu_driver_alive())
 *     input_driver_set_native_keyboard_shown(true);
 *
 * What is counted, textually, in every file under input/ except the
 * frontend itself (input/input_driver.c, its header, and
 * input/input_driver_choice.c):
 *
 *     config_get_ptr()         input_state_get_ptr()
 *     runloop_state_get_ptr()  menu_state_get_ptr()
 *     video_driver_get_ptr()   video_state_get_ptr()
 *
 * allowed[] below was what was still to be converted, file by file with
 * a count, and it is empty now: a file that is not listed may have none,
 * so nothing goes back in.
 *
 * A second thing is counted, the other way round: code outside input/
 * calling an input driver's state function itself,
 *
 *     current_input->input_state(input_st->current_data, joypad, ...)
 *
 * which the menu did, fourteen times, with the driver, its data and the
 * joypads taken out of the input state. It asks the frontend now
 * (input_driver_device_state(), input_driver_bind_capture_state()).
 * The same goes for a joypad driver's functions,
 *
 *     joypad->poll();   joypad->button(port, i);   joypad->name(pad)
 *
 * which the capture of a bind, a driver restart and the saving of a
 * controller profile called (input_driver_capture_pad(),
 * input_driver_poll_devices(), input_driver_autoconfigure_pad()).
 * There is to be none of either.
 *
 * A third: the input state taken by the menu and the UI layer
 * (outside_allowed[]), which are being moved onto the same named calls.
 * The menu drivers take none any more; what is left is listed with a
 * count that only goes down.
 *
 * The rest, each with its reason where its names are listed below: a
 * driver issuing a frontend command, the menu reading the driver for
 * the pointer, a bind array named outside the frontend, a stick worked
 * out from its binds, the binds a driver is handed read or named, and
 * a remap written into the settings directly.
 *
 * Comments are left out of the count; string literals are not.
 *
 * Usage:
 *    tools/check.sh input_state_grab_check [--root DIR] [--selftest] */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

enum counter
{
   C_GRAB = 0,       /* a state pointer taken under input/ */
   C_MENU_READ,      /* the menu calling the driver for the pointer */
   C_COMMAND,        /* a frontend command issued under input/ */
   C_DRIVER_CALL,    /* a driver's function called outside input/ */
   C_BIND_ARRAY,     /* a bind array named outside the frontend */
   C_HANDED,         /* an input driver reading the binds it is handed */
   C_JOYPAD_NAME,    /* a joypad or HID file naming binds */
   C_REMAP,          /* a remap written into the settings directly */
   C_ANALOG,         /* a stick's axis mapped to its binds */
   C_OUTSIDE,        /* the input state taken outside input/ */
   C_COUNT
};

/* what has to follow the name for it to count */
enum follow
{
   F_WORD,           /* nothing: the name on its own */
   F_CALL,           /* \s*\(         */
   F_CALL_EMPTY,     /* \s*\(\s*\)    */
   F_INDEX,          /* \s*\[         */
   F_MEMBER_CALL,    /* \s*->\s*[a-z_]+\s*\(  */
   F_REMAP_WRITE     /* \s*\[[^\]]*\]\s*\[[^\]]*\]\s*(?:[-+|&]?=)(?!=) */
};

struct name_rule
{
   /* in two halves where this file would otherwise be counted itself */
   const char *a;
   const char *b;
   int counter;
   int follow;
};

static const struct name_rule rules[] = {
   { "config_get_ptr", "",          C_GRAB, F_CALL_EMPTY },
   { "input_state_get_ptr", "",     C_GRAB, F_CALL_EMPTY },
   { "runloop_state_get_ptr", "",   C_GRAB, F_CALL_EMPTY },
   { "menu_state_get_ptr", "",      C_GRAB, F_CALL_EMPTY },
   { "video_driver_get_ptr", "",    C_GRAB, F_CALL_EMPTY },
   { "video_state_get_ptr", "",     C_GRAB, F_CALL_EMPTY },
   /* The menu reads the pointer from the view the frontend compiles
    * once a poll (input_driver_pointer_view()) and calls no driver
    * for it. */
   { "input_driver_device_state", "", C_MENU_READ, F_CALL },
   /* Frontend commands an input driver issued itself - quit, shut
    * down, toggle the menu, restart video - which it asks for now with
    * input_driver_platform_request(): there is to be none under input/
    * but in the frontend. */
   { "command_event", "",           C_COMMAND, F_CALL },
   { "retroarch_ctl", "",           C_COMMAND, F_CALL },
   { "retroarch_main_quit", "",     C_COMMAND, F_CALL },
   /* besides ->input_state( , which next_arrow() finds */
   { "joypad", "",                  C_DRIVER_CALL, F_MEMBER_CALL },
   { "sec_joypad", "",              C_DRIVER_CALL, F_MEMBER_CALL },
   { "primary_joypad", "",          C_DRIVER_CALL, F_MEMBER_CALL },
   { "secondary_joypad", "",        C_DRIVER_CALL, F_MEMBER_CALL },
   /* The bind arrays are the frontend input file's own:
    * input_driver.c and the header that declares them. Everything
    * else - the input drivers too - gets at a bind through
    * input_config_bind() and input_autoconf_bind(), and at a whole set
    * through input_config_binds_copy_out() and _copy_in(), so how the
    * binds are kept can change without a driver, the menu, the
    * configuration or the UI changing with it. */
   { "input_config", "_binds",      C_BIND_ARRAY, F_WORD },
   { "input_autoconf", "_binds",    C_BIND_ARRAY, F_WORD },
   /* An input driver is handed the binds with every question it is
    * asked, and none reads them: the pad's buttons, the keys and the
    * mouse buttons a control is bound to are the frontend's to put
    * together (keys_down and bind_mouse_buttons in the driver table
    * give it the keys and the mouse). So how the binds are kept - how
    * many a user has, what a record holds - is no driver's business.
    * A driver may pass the set on; it may not look inside. */
   { "binds", "",                   C_HANDED, F_INDEX },
   { "retro_keybinds", "",          C_HANDED, F_INDEX },
   /* A joypad driver, and an HID backend behind one, is handed no
    * binds at all. Its state call gets two lists - the pad's button
    * and the pad's axis behind each of the RetroPad's sixteen - that
    * the frontend has resolved: the port's own bind, or its
    * controller's profile's. So no file of theirs has a bind's record,
    * a set of them or a profile's to name. */
   { "retro_keybind", "",           C_JOYPAD_NAME, F_WORD },
   { "retro_keybind_set", "",       C_JOYPAD_NAME, F_WORD },
   { "binds", "",                   C_JOYPAD_NAME, F_WORD },
   { "binds_data", "",              C_JOYPAD_NAME, F_WORD },
   { "auto_binds", "",              C_JOYPAD_NAME, F_WORD },
   /* A user's remap of a control is set through
    * input_config_set_remap_id(), which counts it as a change: what
    * the frontend keeps of the remaps - who has anything remapped,
    * which buttons are left as themselves - is made again after a
    * counted change and not otherwise. A remap written into the
    * settings directly would take effect whenever something else
    * happened to be counted. The input's own files and the
    * configuration's loader, which end with a counted call, are where
    * the table is written. */
   { "input_remap_ids", "",         C_REMAP, F_REMAP_WRITE },
   /* Which two binds are behind a stick's axis is the frontend's to
    * know: every input driver used to work a stick out from the keys
    * bound to it, each with the macro that maps the axis to its binds.
    * That lives in input/input_driver.c now (the binds are numbered in
    * a row, so it is arithmetic) and nothing else uses it, by its old
    * name or its new one. */
   { "input_conv_analog_id", "_to_bind_id", C_ANALOG, F_WORD },
   { "INPUT_ANALOG_AXIS", "_BIND",  C_ANALOG, F_WORD },
   { "input_state_get_ptr", "",     C_OUTSIDE, F_CALL_EMPTY }
};

/* the frontend: it owns the input state, and reads the settings for it
 * (input_driver_choice.c is where it picks the input driver by name) */
static const char *const frontend[] = {
   "input/input_driver.c", "input/input_driver.h",
   "input/input_driver_choice.c"
};

struct allowance
{
   const char *rel;
   unsigned long n;
};

/* a file whose own state is kept in one of the six could take that
 * one: { file, name }. There is none: the replay code in input/bsv,
 * whose state is in the input state, is handed it by its callers */
static const char *const own_state[][2] = {
   { NULL, NULL }
};

/* state pointers still taken under input/: { file, count } */
static const struct allowance allowed[] = {
   { NULL, 0 }
};

/* The input state taken by the menu and the UI layer, which are being
 * moved onto named calls as the drivers were: what is left, file by
 * file. A file may not have more than its number, one that is not
 * listed may have none, and a file below its number has it lowered.
 *
 * Nothing under menu/ or ui/ takes it any more: the Apple UI hands its
 * mouse, pointer and touch events to the Cocoa input driver's own calls
 * (input/drivers/cocoa_input.h), and does not write the driver's data.
 *
 * tasks/, network/ and the frontend files below too. What is left there
 * is the replay code being handed the input state by the commands, the
 * save-state and movie tasks and rewind, which is the frontend passing
 * its own state on. */
static const char *const outside_dirs[]  = { "menu", "ui", "tasks", "network" };
static const char *const outside_files[] = {
   "command.c", "configuration.c", "state_manager.c"
};
static const struct allowance outside_allowed[] = {
   { "command.c",           4 },
   { "state_manager.c",     3 },
   { "tasks/task_movie.c",  3 },
   { "tasks/task_save.c",   7 },
   { NULL, 0 }
};

/* where an input driver's own functions are not looked for: the input
 * layer itself, the tests, and code that is not RetroArch's */
static const char *const not_callers[] = {
   "input", "samples", "deps", "libretro-common", "pkg", ".git"
};

struct hit
{
   char *rel;
   unsigned long n;
};

struct hits
{
   struct hit *v;
   size_t n;
   size_t cap;
};

struct tree
{
   struct ck_strv rels;
   const char *root;
};

static char rule_names[COUNT(rules)][40];

static int in_list(const char *const *list, size_t n, const char *s,
      size_t len)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      if (strlen(list[i]) == len && !memcmp(list[i], s, len))
         return 1;
   }
   return 0;
}

static int has_ext(const char *rel, int with_h, int with_mm)
{
   return ck_ends_with(rel, ".c") || ck_ends_with(rel, ".m")
      || ck_ends_with(rel, ".cpp") || (with_mm && ck_ends_with(rel, ".mm"))
      || (with_h && ck_ends_with(rel, ".h"));
}

/* The counters a file is read for. */
static unsigned file_mask(const char *rel)
{
   const char *slash = strchr(rel, '/');
   size_t top_len    = slash ? (size_t)(slash - rel) : 0;
   int ext5          = has_ext(rel, 1, 1);
   int ext4          = has_ext(rel, 0, 1);
   int in_input      = ck_starts_with(rel, "input/");
   int is_frontend   = in_list(frontend, COUNT(frontend), rel, strlen(rel));
   /* not RetroArch's own code, or its tests */
   int foreign       = top_len && in_list(not_callers + 1,
         COUNT(not_callers) - 1, rel, top_len);
   unsigned mask     = 0;

   if (in_input && ext5 && !is_frontend)
      mask |= 1u << C_GRAB;
   if (ck_starts_with(rel, "menu/") && has_ext(rel, 0, 0))
      mask |= 1u << C_MENU_READ;
   if (in_input && ext4 && !is_frontend)
      mask |= 1u << C_COMMAND;
   if (ext4 && !in_input && !foreign)
      mask |= 1u << C_DRIVER_CALL;
   if (ext5 && !foreign && strcmp(rel, "input/input_driver.c")
         && strcmp(rel, "input/input_driver.h"))
      mask |= 1u << C_BIND_ARRAY;
   if (ext5 && ck_starts_with(rel, "input/drivers/")
         && !strchr(rel + strlen("input/drivers/"), '/'))
      mask |= 1u << C_HANDED;
   if (ext5 && (ck_starts_with(rel, "input/drivers_joypad/")
            || ck_starts_with(rel, "input/drivers_hid/")))
      mask |= 1u << C_JOYPAD_NAME;
   /* libretro-common and pkg are read for this one */
   if (ext5 && !in_input && strcmp(rel, "configuration.c"))
      mask |= 1u << C_REMAP;
   if (ext5 && !foreign && strcmp(rel, "input/input_driver.c"))
      mask |= 1u << C_ANALOG;
   if (in_list(outside_files, COUNT(outside_files), rel, strlen(rel)))
      mask |= 1u << C_OUTSIDE;
   else if (ext4 && top_len
         && in_list(outside_dirs, COUNT(outside_dirs), rel, top_len))
   {
      /* a test directory below one of them is left out */
      const char *p = slash;
      int test      = 0;
      while (p && !test)
      {
         const char *next = strchr(p + 1, '/');
         test = next && next - p == 5 && !memcmp(p + 1, "test", 4);
         p    = next;
      }
      if (!test)
         mask |= 1u << C_OUTSIDE;
   }
   return mask;
}

/* Comments out: a block comment becomes spaces, its line breaks kept,
 * and then a line comment is cut off its line. An unclosed block
 * comment stays, and neither pass knows a string literal. */
static void strip_comments(char *t, size_t *len)
{
   size_t n = *len;
   size_t i, o;

   for (i = 0; i + 1 < n; i++)
   {
      size_t j;
      if (t[i] != '/' || t[i + 1] != '*')
         continue;
      for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
      if (j + 1 >= n)
         break;
      for (; i < j + 2; i++)
      {
         if (t[i] != '\n')
            t[i] = ' ';
      }
      i--;
   }
   for (i = o = 0; i < n; )
   {
      if (t[i] == '/' && i + 1 < n && t[i + 1] == '/')
      {
         while (i < n && t[i] != '\n')
            i++;
      }
      else
         t[o++] = t[i++];
   }
   t[o] = '\0';
   *len = o;
}

static const char *skip_space(const char *p, const char *end)
{
   while (p < end && CK_IS_SPACE(*p))
      p++;
   return p;
}

/* Where a match that began with a name ends, or NULL. p is the first
 * character after the name. */
static const char *match_follow(int follow, const char *p, const char *end)
{
   const char *q;
   switch (follow)
   {
      case F_WORD:
         return p;
      case F_CALL:
         p = skip_space(p, end);
         return p < end && *p == '(' ? p + 1 : NULL;
      case F_CALL_EMPTY:
         p = skip_space(p, end);
         if (p >= end || *p != '(')
            return NULL;
         p = skip_space(p + 1, end);
         return p < end && *p == ')' ? p + 1 : NULL;
      case F_INDEX:
         p = skip_space(p, end);
         return p < end && *p == '[' ? p + 1 : NULL;
      case F_MEMBER_CALL:
         p = skip_space(p, end);
         if (end - p < 2 || p[0] != '-' || p[1] != '>')
            return NULL;
         p = skip_space(p + 2, end);
         for (q = p; q < end && ((*q >= 'a' && *q <= 'z') || *q == '_'); q++);
         if (q == p)
            return NULL;
         q = skip_space(q, end);
         return q < end && *q == '(' ? q + 1 : NULL;
      default:
         {
            int i;
            for (i = 0; i < 2; i++)
            {
               p = skip_space(p, end);
               if (p >= end || *p != '[')
                  return NULL;
               q = (const char*)memchr(p, ']', (size_t)(end - p));
               if (!q)
                  return NULL;
               p = q + 1;
            }
            p = skip_space(p, end);
            if (p < end && (*p == '-' || *p == '+' || *p == '|' || *p == '&'))
               p++;
            if (p >= end || *p != '=' || (p + 1 < end && p[1] == '='))
               return NULL;
            return p + 1;
         }
   }
}

/* ->\s*input_state\s*\(  with p at the arrow */
static const char *next_arrow(const char *p, const char *end)
{
   static const char name[] = "input_state";
   p = skip_space(p + 2, end);
   if ((size_t)(end - p) < sizeof(name) - 1
         || memcmp(p, name, sizeof(name) - 1))
      return NULL;
   p = skip_space(p + sizeof(name) - 1, end);
   return p < end && *p == '(' ? p + 1 : NULL;
}

/* One pass over a file for every counter in mask. A counter's matches
 * do not overlap: the next may start where the last one ended. own is
 * the one name the file may take, or NULL. */
static void count_file(const char *t, size_t len, unsigned mask,
      const char *own, unsigned long *counts)
{
   const char *end = t + len;
   const char *resume[C_COUNT];
   const char *p   = t;
   size_t i;

   for (i = 0; i < C_COUNT; i++)
   {
      resume[i] = t;
      counts[i] = 0;
   }
   while (p < end)
   {
      const char *w;
      size_t wl;
      if (!CK_IS_WORD(*p))
      {
         if (*p == '-' && (mask & (1u << C_DRIVER_CALL)) && end - p >= 2
               && p[1] == '>' && p >= resume[C_DRIVER_CALL])
         {
            const char *e = next_arrow(p, end);
            if (e)
            {
               counts[C_DRIVER_CALL]++;
               resume[C_DRIVER_CALL] = e;
            }
         }
         p++;
         continue;
      }
      for (w = p; p < end && CK_IS_WORD(*p); p++);
      wl = (size_t)(p - w);
      /* every name is 5 to 31 letters long */
      if (wl < 5 || wl > 31)
         continue;
      for (i = 0; i < COUNT(rules); i++)
      {
         int c = rules[i].counter;
         const char *e;
         if (     !(mask & (1u << c)) || w < resume[c]
               || rule_names[i][0] != *w || strlen(rule_names[i]) != wl
               || memcmp(rule_names[i], w, wl)
               || !(e = match_follow(rules[i].follow, p, end)))
            continue;
         resume[c] = e;
         if (c != C_GRAB || !own || strcmp(own, rule_names[i]))
            counts[c]++;
      }
   }
}

static int skip_dir(const char *name, const char *rel, void *ud)
{
   (void)ud;
   /* nothing is counted under these three */
   return !strchr(rel, '/') && (!strcmp(name, ".git")
         || !strcmp(name, "deps") || !strcmp(name, "samples"));
}

static void add_file(const char *path, const char *rel, void *ud)
{
   struct tree *tr = (struct tree*)ud;
   (void)path;
   if (file_mask(rel))
      ck_strv_push(&tr->rels, ck_strdup(rel));
}

static int cmp_hit(const void *a, const void *b)
{
   return strcmp(((const struct hit*)a)->rel, ((const struct hit*)b)->rel);
}

static void hits_free(struct hits *h)
{
   size_t i;
   for (i = 0; i < C_COUNT; i++)
   {
      size_t k;
      for (k = 0; k < h[i].n; k++)
         free(h[i].v[k].rel);
      free(h[i].v);
   }
}

/* Counts the tree: for each counter wanted, the files with a count,
 * sorted by name. */
static void scan(const char *root, unsigned want, struct hits *h)
{
   struct tree tr;
   struct ck_sb path = {0};
   size_t i, c;

   memset(&tr, 0, sizeof(tr));
   memset(h, 0, C_COUNT * sizeof(*h));
   tr.root = root;
   ck_walk(root, skip_dir, add_file, &tr);
   for (i = 0; i < tr.rels.n; i++)
   {
      unsigned long counts[C_COUNT];
      const char *rel = tr.rels.v[i];
      unsigned mask   = file_mask(rel) & want;
      const char *own = NULL;
      size_t len, k;
      char *text;

      if (!mask)
         continue;
      path.len = 0;
      ck_sb_puts(&path, root);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, rel);
      if (!(text = ck_read_file(path.p, &len)))
         continue;
      strip_comments(text, &len);
      for (k = 0; own_state[k][0]; k++)
      {
         if (!strcmp(own_state[k][0], rel))
            own = own_state[k][1];
      }
      count_file(text, len, mask, own, counts);
      free(text);
      for (c = 0; c < C_COUNT; c++)
      {
         if (!counts[c])
            continue;
         if (h[c].n == h[c].cap)
         {
            h[c].cap = h[c].cap ? h[c].cap * 2 : 16;
            h[c].v   = (struct hit*)ck_realloc(h[c].v,
                  h[c].cap * sizeof(struct hit));
         }
         h[c].v[h[c].n].rel = ck_strdup(rel);
         h[c].v[h[c].n].n   = counts[c];
         h[c].n++;
      }
   }
   for (c = 0; c < C_COUNT; c++)
   {
      if (h[c].n > 1)
         qsort(h[c].v, h[c].n, sizeof(struct hit), cmp_hit);
   }
   ck_strv_free(&tr.rels);
   ck_sb_free(&path);
}

static unsigned long hit_count(const struct hits *h, const char *rel)
{
   size_t i;
   for (i = 0; i < h->n; i++)
   {
      if (!strcmp(h->v[i].rel, rel))
         return h->v[i].n;
   }
   return 0;
}

static unsigned long allowance(const struct allowance *a, const char *rel)
{
   for (; a->rel; a++)
   {
      if (!strcmp(a->rel, rel))
         return a->n;
   }
   return 0;
}

/* Files that have more than they may, and files that have less than
 * their line still says: the found and the listed, in name order. */
static int over_and_under(const struct hits *h, const struct allowance *a,
      const char *over, const char *under, FILE *out)
{
   struct ck_strv names = {0};
   const struct allowance *e;
   size_t i;
   int bad = 0;

   for (i = 0; i < h->n; i++)
      ck_strv_push(&names, ck_strdup(h->v[i].rel));
   for (e = a; e->rel; e++)
   {
      if (!hit_count(h, e->rel))
         ck_strv_push(&names, ck_strdup(e->rel));
   }
   ck_strv_sort(&names);
   for (i = 0; i < names.n; i++)
   {
      unsigned long have = hit_count(h, names.v[i]);
      unsigned long may  = allowance(a, names.v[i]);
      if (i && !strcmp(names.v[i], names.v[i - 1]))
         continue;
      if (have > may)
      {
         if (out)
            fprintf(out, over, names.v[i], have, may);
         bad++;
      }
      else if (have < may)
      {
         if (out)
            fprintf(out, under, names.v[i], have, may,
                  have ? "" : " (remove the line)");
         bad++;
      }
   }
   ck_strv_free(&names);
   return bad;
}

static int report(const struct hits *h, const char *msg, FILE *out)
{
   size_t i;
   for (i = 0; out && i < h->n; i++)
      fprintf(out, msg, h->v[i].rel, h->v[i].n);
   return (int)h->n;
}

/* out is NULL for a run that only wants the result. */
static int run(const char *root, const struct allowance *may,
      const struct allowance *outside_may, FILE *out)
{
   struct hits h[C_COUNT];
   unsigned long left = 0, outside = 0;
   size_t i;
   int bad = 0;

   scan(root, ~0u, h);
   bad += over_and_under(&h[C_OUTSIDE], outside_may,
         "%s: takes the input state %lu time(s), %lu allowed.\n"
         "  Ask the frontend by name (input_driver_...()).\n",
         "%s: down to %lu, and outside_allowed[] in"
         " tools/input_state_grab_check.c still says %lu.\n"
         "  Lower the number%s.\n", out);
   bad += report(&h[C_MENU_READ],
         "%s: reads the input driver for the pointer itself, %lu time(s).\n"
         "  Read the view the frontend compiles once a poll: "
         "input_driver_pointer_view().\n", out);
   bad += report(&h[C_COMMAND],
         "%s: issues a frontend command itself, %lu time(s).\n"
         "  Ask for it: input_driver_platform_request().\n", out);
   bad += report(&h[C_DRIVER_CALL],
         "%s: calls an input or joypad driver itself, %lu time(s).\n"
         "  Ask the frontend: input_driver_device_state(), "
         "input_driver_capture_pad(), input_driver_poll_devices().\n", out);
   bad += report(&h[C_ANALOG],
         "%s: maps a stick's axis to its binds itself, %lu time(s).\n"
         "  The frontend answers a stick from the keys bound to it: give it\n"
         "  the keys (keys_down in the driver table).\n", out);
   bad += report(&h[C_REMAP],
         "%s: writes a remap into the settings directly, %lu time(s).\n"
         "  Set it with input_config_set_remap_id(user, id, remap), which\n"
         "  counts the change; what the frontend keeps of the remaps is made\n"
         "  again only after a counted change.\n", out);
   bad += report(&h[C_JOYPAD_NAME],
         "%s: names binds, %lu time(s).\n"
         "  A joypad driver is handed what is behind each of the RetroPad's\n"
         "  sixteen on its pad (joykeys, joyaxes in its state call), resolved\n"
         "  by the frontend; it has no binds to read or to pass on.\n", out);
   bad += report(&h[C_HANDED],
         "%s: reads the binds it is handed, %lu time(s).\n"
         "  The frontend answers what is bound: give it the keys and the\n"
         "  mouse (keys_down, bind_mouse_buttons in the driver table).\n", out);
   bad += report(&h[C_BIND_ARRAY],
         "%s: names a bind array itself, %lu time(s).\n"
         "  Get at a bind through input_config_bind() or "
         "input_autoconf_bind().\n", out);
   bad += over_and_under(&h[C_GRAB], may,
         "%s: takes a state pointer %lu time(s), %lu allowed.\n"
         "  Ask by name instead: a call on the side that owns the state "
         "(input_driver_...(), menu_driver_alive(), runloop_get_flags()).\n",
         "%s: down to %lu, and the list in"
         " tools/input_state_grab_check.c still says %lu.\n"
         "  Lower the number%s.\n", out);
   for (i = 0; i < h[C_GRAB].n; i++)
      left += h[C_GRAB].v[i].n;
   for (i = 0; i < h[C_OUTSIDE].n; i++)
      outside += h[C_OUTSIDE].v[i].n;
   if (out)
   {
      if (bad)
         fprintf(out, "FAIL input_state_grab_check: %d file(s)\n", bad);
      else
         fprintf(out, "PASS input_state_grab_check (under input/: %lu state"
               " pointer(s) taken; outside it: the input state taken %lu"
               " time(s) in %lu file(s), none new)\n", left, outside,
               (unsigned long)h[C_OUTSIDE].n);
   }
   hits_free(h);
   return bad ? 1 : 0;
}

/* self-test: fixtures under a temporary root. A line that the tree
 * check would count in this file is written in two pieces. */

static void put(const char *root, const char *rel, const char *text,
      const char *more, int append)
{
   struct ck_sb path = {0};
   FILE *f;
   ck_sb_puts(&path, root);
   ck_sb_putc(&path, '/');
   ck_sb_puts(&path, rel);
   ck_mkdirs_for(path.p);
   if ((f = fopen(path.p, append ? "ab" : "wb")))
   {
      fputs(text, f);
      if (more)
         fputs(more, f);
      fclose(f);
   }
   ck_sb_free(&path);
}

static void drop(const char *root, const char *rel)
{
   struct ck_sb path = {0};
   ck_sb_puts(&path, root);
   ck_sb_putc(&path, '/');
   ck_sb_puts(&path, rel);
   remove(path.p);
   ck_sb_free(&path);
}

/* Whether one counter found exactly the files listed (rel, count
 * pairs ended by NULL). */
static int found_is(const char *root, int counter, const struct allowance *want)
{
   struct hits h[C_COUNT];
   const struct allowance *e;
   size_t n = 0;
   int ok   = 1;

   scan(root, 1u << counter, h);
   for (e = want; e->rel; e++, n++)
      ok &= hit_count(&h[counter], e->rel) == e->n;
   ok &= h[counter].n == n;
   hits_free(h);
   return ok;
}

static int selftest(void)
{
   static const struct allowance none[]     = { { NULL, 0 } };
   static const struct allowance b2[]       = {
      { "input/drivers/b_input.c", 2 }, { NULL, 0 } };
   static const struct allowance b1[]       = {
      { "input/drivers/b_input.c", 1 }, { NULL, 0 } };
   static const struct allowance b3[]       = {
      { "input/drivers/b_input.c", 3 }, { NULL, 0 } };
   static const struct allowance b2a1[]     = {
      { "input/drivers/b_input.c", 2 }, { "input/drivers/a_input.c", 1 },
      { NULL, 0 } };
   static const struct allowance u1[]       = { { "ui/u.c", 1 }, { NULL, 0 } };
   static const struct allowance u2[]       = { { "ui/u.c", 2 }, { NULL, 0 } };
   static const struct allowance u3[]       = { { "ui/u.c", 3 }, { NULL, 0 } };
   static const struct allowance one_m[]    = { { "menu/m.c", 1 }, { NULL, 0 } };
   static const struct allowance one_cmd[]  = {
      { "input/drivers/cmd.c", 1 }, { NULL, 0 } };
   static const struct allowance two_b[]    = { { "menu/b.c", 2 }, { NULL, 0 } };
   static const struct allowance one_bn[]   = {
      { "input/drivers/bn.c", 1 }, { NULL, 0 } };
   static const struct allowance one_hb[]   = {
      { "input/drivers/hb.c", 1 }, { NULL, 0 } };
   static const struct allowance joypads[]  = {
      { "input/drivers_joypad/sub/jb.c", 3 }, { "input/drivers_hid/hb.c", 1 },
      { NULL, 0 } };
   static const struct allowance one_rw[]   = {
      { "menu/cbs/rw.c", 1 }, { NULL, 0 } };
   static const struct allowance one_an[]   = {
      { "input/drivers/an.c", 1 }, { NULL, 0 } };
   struct
   {
      const struct allowance *may;
      int want;
      const char *what;
   } cases[6], outside_cases[4];
   char *root = ck_tmpdir();
   int bad    = 0;
   size_t i;

   cases[0].may = b2;   cases[0].want = 0; cases[0].what = "the listed count";
   cases[1].may = b1;   cases[1].want = 1; cases[1].what = "one more than listed";
   cases[2].may = none; cases[2].want = 1; cases[2].what = "a file that is not listed";
   cases[3].may = b2;   cases[3].want = 1;
   cases[3].what = "a menu file calling the driver";
   cases[4].may = b3;   cases[4].want = 1; cases[4].what = "fewer than listed";
   cases[5].may = b2a1; cases[5].want = 1; cases[5].what = "a listed file with none left";
   outside_cases[0].may = u2;   outside_cases[0].want = 0;
   outside_cases[0].what = "a UI file at its number";
   outside_cases[1].may = u1;   outside_cases[1].want = 1;
   outside_cases[1].what = "a UI file over its number";
   outside_cases[2].may = u3;   outside_cases[2].want = 1;
   outside_cases[2].what = "a UI file under its number";
   outside_cases[3].may = none; outside_cases[3].want = 1;
   outside_cases[3].what = "a UI file that is not listed";

   put(root, "input/input_driver.c", "settings_t *s = config_get_ptr();\n",
         NULL, 0);
   put(root, "input/drivers/a_input.c", "if (menu_driver_alive()) x();\n"
         "/* config_get_ptr() in a comment */\n", NULL, 0);
   put(root, "input/drivers/b_input.c", "x = config_get_ptr ( )->uints.y;\n"
         "input_state_get_ptr()->flags |= 1;\n", NULL, 0);
   for (i = 0; i < 6; i++)
   {
      int got;
      if (i == 3)
         put(root, "menu/m.c", "x = current_input->input",
               "_state(data, 0);\n", 0);
      got = run(root, cases[i].may, none, NULL);
      if (i == 3)
         drop(root, "menu/m.c");
      if (got != cases[i].want)
      {
         printf("selftest: %s: wanted %d, got %d\n", cases[i].what,
               cases[i].want, got);
         bad++;
      }
   }
   /* the menu and UI lists: a file over its number, and one under it */
   put(root, "ui/u.c", "a = input_state_get_ptr()->flags;\n"
         "b = input_state_get_ptr();\n", NULL, 0);
   for (i = 0; i < 4; i++)
   {
      int got = run(root, b2, outside_cases[i].may, NULL);
      if (got != outside_cases[i].want)
      {
         printf("selftest: %s: wanted %d, got %d\n", outside_cases[i].what,
               outside_cases[i].want, got);
         bad++;
      }
   }
   /* the menu reading the driver for the pointer itself */
   put(root, "menu/m.c", "int m(void) { return input_driver_device_state"
         "(0, 2, 0, 0); }\n", NULL, 0);
   if (!found_is(root, C_MENU_READ, one_m))
   {
      printf("selftest: the menu reading the driver for the pointer was"
            " not found\n");
      bad++;
   }
   drop(root, "menu/m.c");
   /* a driver issuing a frontend command itself, and one asking */
   put(root, "input/drivers/cmd.c",
         "void q(void) { command_event(CMD_EVENT_QUIT, 0); }\n", NULL, 0);
   if (!found_is(root, C_COMMAND, one_cmd))
   {
      printf("selftest: a driver issuing a frontend command was not found\n");
      bad++;
   }
   put(root, "input/drivers/cmd.c", "void q(void) { input_driver_platform"
         "_request(INPUT_PLATFORM_QUIT); }\n", NULL, 0);
   if (!found_is(root, C_COMMAND, none))
   {
      printf("selftest: a driver asking for a quit was taken for one"
            " issuing it\n");
      bad++;
   }
   /* code outside input/ naming a bind array, and going through the call */
   put(root, "menu/b.c", "x = input_config" "_binds[0][1].joykey;\n"
         "y = &input_autoconf" "_binds[p][i];\n",
         "input_config_binds_copy_out(sets); /* input_config" "_binds */\n",
         0);
   if (!found_is(root, C_BIND_ARRAY, two_b))
   {
      printf("selftest: a menu file naming the bind arrays was not counted"
            " as two\n");
      bad++;
   }
   put(root, "menu/b.c", "x = input_config_bind(0, 1)->joykey;\n"
         "y = input_autoconf_bind(p, i);\n", NULL, 0);
   if (!found_is(root, C_BIND_ARRAY, none))
   {
      printf("selftest: a menu file going through the calls was taken for"
            " one naming the arrays\n");
      bad++;
   }
   drop(root, "menu/b.c");
   /* an input driver naming them is counted; the frontend's file is not */
   put(root, "input/drivers/bn.c", "x = input_autoconf" "_binds[0][1].joykey;\n",
         NULL, 0);
   put(root, "input/input_driver.c", "y = input_config" "_binds[0][1].joykey;\n",
         NULL, 1);
   if (!found_is(root, C_BIND_ARRAY, one_bn))
   {
      printf("selftest: an input driver naming the bind arrays was not the"
            " one file counted\n");
      bad++;
   }
   drop(root, "input/drivers/bn.c");
   /* a driver working a stick out from its binds itself */
   put(root, "input/drivers/an.c", "input_conv_analog_id" "_to_bind_id",
         "(idx, id, id_minus, id_plus);\n", 0);
   put(root, "input/input_driver.c", "input_conv_analog_id" "_to_bind_id",
         "(idx, id, a, b);\n", 1);
   /* a driver looking inside the binds it is handed; passing them on
    * is not */
   put(root, "input/drivers/hb.c",
         "if (binds[port][id].joykey != NO_BTN) return 1;\n"
         "return other_state(udev, binds, port); /* binds[port] */\n",
         NULL, 0);
   if (!found_is(root, C_HANDED, one_hb))
   {
      printf("selftest: a driver reading the binds it is handed was not"
            " the one read counted\n");
      bad++;
   }
   drop(root, "input/drivers/hb.c");
   /* a joypad driver choosing between a bind and the profile's, and an
    * HID backend taking binds; one that goes by its two lists is
    * neither */
   put(root, "input/drivers_joypad/sub/jb.c",
         "k = (binds[i].joykey != NO_BTN) ? binds[i].joykey\n"
         "   : joypad_info->auto_binds[i].joykey;\n", NULL, 0);
   put(root, "input/drivers_hid/hb.c", "static int16_t st(void *d,"
         " const void *binds_data) { return 0; }\n", NULL, 0);
   put(root, "input/drivers_joypad/ok.c",
         "/* no binds here */ k = joykeys[i]; a = joyaxes[i];\n", NULL, 0);
   if (!found_is(root, C_JOYPAD_NAME, joypads))
   {
      printf("selftest: joypad and HID files naming binds were not the two"
            " counted\n");
      bad++;
   }
   /* a remap written straight into the settings by the menu; the same
    * write in the input's own file, a read and a comparison are not
    * it */
   put(root, "menu/cbs/rw.c", "settings->uints.input_remap_ids",
         "[u][b] = idx;\n"
         "if (settings->uints.input_remap_ids[u][b] == idx) x = 1;\n"
         "y = settings->uints.input_remap_ids[u][b];\n", 0);
   put(root, "input/ours.c", "settings->uints.input_remap_ids",
         "[u][b] = idx;\n", 0);
   if (!found_is(root, C_REMAP, one_rw))
   {
      printf("selftest: a remap written directly was not the one counted\n");
      bad++;
   }
   if (!found_is(root, C_ANALOG, one_an))
   {
      printf("selftest: a driver mapping a stick to its binds was not found,"
            " or the frontend was taken for one\n");
      bad++;
   }
   drop(root, "input/drivers/an.c");
   ck_rmtree(root);
   free(root);
   if (bad)
   {
      printf("FAIL input_state_grab_check --selftest\n");
      return 1;
   }
   printf("PASS input_state_grab_check --selftest (%d cases)\n", 6 + 13);
   return 0;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   int self         = 0;
   size_t r;
   int i;

   for (r = 0; r < COUNT(rules); r++)
   {
      strcpy(rule_names[r], rules[r].a);
      strcat(rule_names[r], rules[r].b);
   }
   if (!root || !*root)
      root = ".";
   for (i = 1; i < argc; i++)
   {
      if (!strcmp(argv[i], "--selftest"))
         self = 1;
      else if (!strcmp(argv[i], "--root") && i + 1 < argc)
         root = argv[++i];
      else if (!strncmp(argv[i], "--root=", 7))
         root = argv[i] + 7;
      else
      {
         fprintf(stderr, "usage: input_state_grab_check [--root DIR]"
               " [--selftest]\n");
         return 2;
      }
   }
   return self ? selftest() : run(root, allowed, outside_allowed, stdout);
}

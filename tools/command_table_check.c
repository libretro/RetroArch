/* Check that every command in command.h describes itself.
 *
 * The command tables in command.h are the one list of what RetroArch
 * can be told to do. HELP prints them, and interfaces that hand the
 * commands to a client (tool lists, key binding help) are generated from
 * them, so a command added without a description reaches all of those
 * as a blank. This fails on any row of action_map or map that has no
 * description, an empty one, or an unknown flag.
 *
 * A command that takes an argument also has to describe it as
 * parameters, or a structured client is given a tool it cannot call
 * properly; one that takes none must have NULL there.
 *
 * Rows are matched as C initializers:
 *   action_map: { "NAME", handler, "arg", "description", flags, params, rows },
 *   map:        { "NAME", ID, "description", flags },
 *
 * Usage, from the repository root: tools/check.sh command_table_check */

#include "check_common.h"

#define HEADER "command.h"

static const char *const flag_names[] = {
   "0", "CMD_INFO_DESTRUCTIVE", "CMD_INFO_NO_TOOL", "CMD_INFO_READ_ONLY"
};

#define FLAGS_CLASS(c) (CK_IS_WORD(c) || (c) == '|' || CK_IS_SPACE(c))

/* A part of a row: where it is, or b NULL when the row has none. */
struct span
{
   const char *b;
   const char *e;
};

struct row
{
   struct span name;
   struct span arg;
   struct span desc;
   struct span flags;
   struct span params;
   const char *end;
};

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

/* "((?:[^"\\]|\\.)*)"  at p: the end, or NULL */
static const char *escaped_literal(const char *p, const char *e,
      struct span *s)
{
   const char *q;
   if (p >= e || *p != '"')
      return NULL;
   for (q = p + 1; q < e && *q != '"'; q++)
   {
      if (*q == '\\')
      {
         if (q + 1 >= e || q[1] == '\n')
            return NULL;
         q++;
      }
   }
   if (q >= e)
      return NULL;
   s->b = p + 1;
   s->e = q;
   return q + 1;
}

/* What follows the description: (?:,\s*([\w|\s]+?)\s*)? and, for
 * action_map, (?:,\s*(\w+)\s*)?, then the closing brace - in the order
 * the expression tries them. */
/* An optional field after the parameters that this check does not read:
 * the shape of the command's reply, "&cmd_rows_x". Gives back what it
 * was handed where there is none. */
static const char *skip_rows(const char *u, const char *e)
{
   const char *v;
   const char *w;
   if (u >= e || *u != ',')
      return u;
   v = ws(u + 1, e);
   if (v < e && *v == '&')
      v++;
   for (w = v; v < e && CK_IS_WORD(*v); v++);
   return v > w ? ws(v, e) : u;
}

static const char *tail(const char *p, const char *e, int with_params,
      struct row *r)
{
   int take;
   for (take = 1; take >= 0; take--)
   {
      const char *s;
      long k;
      r->flags.b = NULL;
      if (take)
      {
         const char *a;
         if (p >= e || *p != ',')
            continue;
         a = p + 1;
         s = ws(a, e);
         /* \s* gives back what it took, one byte at a time */
         for (k = (long)(s - a); k >= 0; k--)
         {
            const char *fb = a + k;
            const char *fe;
            for (fe = fb + 1; fe <= e && FLAGS_CLASS(fe[-1]); fe++)
            {
               int ptake;
               const char *q = ws(fe, e);
               for (ptake = with_params; ptake >= 0; ptake--)
               {
                  const char *u = q;
                  r->params.b = NULL;
                  if (ptake)
                  {
                     const char *w;
                     if (u >= e || *u != ',')
                        continue;
                     u = ws(u + 1, e);
                     for (w = u; u < e && CK_IS_WORD(*u); u++);
                     if (u == w)
                        continue;
                     r->params.b = w;
                     r->params.e = u;
                     u = ws(u, e);
                  }
                  u = skip_rows(u, e);
                  if (u < e && *u == '}')
                  {
                     r->flags.b = fb;
                     r->flags.e = fe;
                     return u + 1;
                  }
               }
               if (fe >= e)
                  break;
            }
         }
         continue;
      }
      {
         int ptake;
         for (ptake = with_params; ptake >= 0; ptake--)
         {
            const char *u = p;
            r->params.b = NULL;
            if (ptake)
            {
               const char *w;
               if (u >= e || *u != ',')
                  continue;
               u = ws(u + 1, e);
               for (w = u; u < e && CK_IS_WORD(*u); u++);
               if (u == w)
                  continue;
               r->params.b = w;
               r->params.e = u;
               u = ws(u, e);
            }
            u = skip_rows(u, e);
            if (u < e && *u == '}')
               return u + 1;
         }
      }
   }
   return NULL;
}

static int is_name_char(int c)
{
   return (c >= 'A' && c <= 'Z') || CK_IS_DIGIT(c) || c == '_';
}

/* A row at p, which is '{':
 *   action_map: \{\s*"([A-Z0-9_]+)"\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*
 *               (?:,\s*"((?:[^"\\]|\\.)*)"\s*)?(?:,\s*([\w|\s]+?)\s*)?
 *               (?:,\s*(\w+)\s*)?\}
 *   map:        \{\s*"([A-Z0-9_]+)"\s*,\s*([A-Z0-9_]+)\s*
 *               (?:,\s*"((?:[^"\\]|\\.)*)"\s*)?(?:,\s*([\w|\s]+?)\s*)?\} */
static int row_at(const char *p, const char *e, int action, struct row *r)
{
   const char *q = ws(p + 1, e);
   const char *w;
   int take;

   memset(r, 0, sizeof(*r));
   if (q >= e || *q != '"')
      return 0;
   for (w = ++q; q < e && is_name_char(*q); q++);
   if (q == w || q >= e || *q != '"')
      return 0;
   r->name.b = w;
   r->name.e = q;
   q = ws(q + 1, e);
   if (q >= e || *q != ',')
      return 0;
   q = ws(q + 1, e);
   for (w = q; q < e && (action ? CK_IS_WORD(*q) : is_name_char(*q)); q++);
   if (q == w)
      return 0;
   q = ws(q, e);
   if (action)
   {
      if (q >= e || *q != ',')
         return 0;
      q = ws(q + 1, e);
      if (q >= e || *q != '"')
         return 0;
      for (w = ++q; q < e && *q != '"'; q++);
      if (q >= e)
         return 0;
      r->arg.b = w;
      r->arg.e = q;
      q = ws(q + 1, e);
   }
   for (take = 1; take >= 0; take--)
   {
      const char *u = q;
      const char *end;
      r->desc.b = NULL;
      if (take)
      {
         if (u >= e || *u != ',')
            continue;
         u = ws(u + 1, e);
         if (!(u = escaped_literal(u, e, &r->desc)))
            continue;
         u = ws(u, e);
      }
      if ((end = tail(u, e, action, r)))
      {
         r->end = end;
         return 1;
      }
   }
   return 0;
}

static void put_span_repr(struct ck_sb *sb, const struct span *s)
{
   if (!s->b)
      ck_sb_puts(sb, "None");
   else
      ck_repr(sb, s->b, s->e);
}

static int span_is(const struct span *s, const char *str)
{
   return s->b && (size_t)(s->e - s->b) == strlen(str)
      && !memcmp(s->b, str, strlen(str));
}

static void trim(const char **b, const char **e)
{
   while (*b < *e && (CK_IS_SPACE(**b) || (**b >= 0x1c && **b <= 0x1f)))
      (*b)++;
   while (*e > *b && (CK_IS_SPACE((*e)[-1])
            || ((*e)[-1] >= 0x1c && (*e)[-1] <= 0x1f)))
      (*e)--;
}

static void check_flags(const struct row *r, struct ck_strv *bad)
{
   int ok = r->flags.b && r->flags.e > r->flags.b;
   if (ok)
   {
      const char *p = r->flags.b;
      while (ok)
      {
         const char *bar = (const char*)memchr(p, '|',
               (size_t)(r->flags.e - p));
         const char *b   = p;
         const char *e   = bar ? bar : r->flags.e;
         size_t i;
         int known       = 0;
         trim(&b, &e);
         for (i = 0; i < sizeof(flag_names) / sizeof(flag_names[0]); i++)
            known |= (size_t)(e - b) == strlen(flag_names[i])
               && !memcmp(b, flag_names[i], (size_t)(e - b));
         ok = known;
         if (!bar)
            break;
         p = bar + 1;
      }
   }
   if (!ok)
   {
      struct ck_sb m = {0};
      ck_sb_add(&m, r->name.b, (size_t)(r->name.e - r->name.b));
      ck_sb_puts(&m, ": flags ");
      put_span_repr(&m, &r->flags);
      ck_sb_puts(&m, " are not a combination of 0, CMD_INFO_DESTRUCTIVE,"
            " CMD_INFO_NO_TOOL, CMD_INFO_READ_ONLY");
      ck_strv_push(bad, m.p);
   }
}

static void check_desc(const struct row *r, struct ck_strv *bad)
{
   const char *b, *e;
   if (r->desc.b)
   {
      b = r->desc.b;
      e = r->desc.e;
      trim(&b, &e);
      if (b < e)
         return;
   }
   {
      struct ck_sb m = {0};
      ck_sb_add(&m, r->name.b, (size_t)(r->name.e - r->name.b));
      ck_sb_puts(&m, ": no description");
      ck_strv_push(bad, m.p);
   }
}

/* The table that starts with head, as far as the next "};". */
static int table(const char *text, const char *head, const char **b,
      const char **e)
{
   const char *s = strstr(text, head);
   if (!s)
      return 0;
   if (!(*e = strstr(s, "};")))
   {
      fprintf(stderr, "command_table_check: a table in %s does not end\n",
            HEADER);
      exit(1);
   }
   *b = s;
   return 1;
}

int main(int argc, char **argv)
{
   struct ck_strv bad = {0};
   unsigned long count = 0;
   const char *ab, *ae, *hb, *he, *p;
   size_t len, i;
   char *text;
   int action;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: command_table_check\n");
      return 2;
   }
   if (!(text = ck_read_file(HEADER, &len)))
   {
      fprintf(stderr, "command_table_check: cannot read %s\n", HEADER);
      return 1;
   }
   if (     !table(text, "struct cmd_action_map action_map[] = {", &ab, &ae)
         || !table(text, "struct cmd_map map[] = {", &hb, &he))
   {
      printf("command_table_check: tables not found in %s\n", HEADER);
      return 1;
   }
   for (action = 1; action >= 0; action--)
   {
      const char *b = action ? ab : hb;
      const char *e = action ? ae : he;
      for (p = b; (p = (const char*)memchr(p, '{', (size_t)(e - p))); )
      {
         struct row r;
         if (!row_at(p, e, action, &r))
         {
            p++;
            continue;
         }
         count++;
         check_desc(&r, &bad);
         check_flags(&r, &bad);
         if (action)
         {
            int takes_arg = !span_is(&r.arg, "No argument");
            int none      = !r.params.b || span_is(&r.params, "NULL");
            struct ck_sb m = {0};
            if (takes_arg && none)
            {
               ck_sb_add(&m, r.name.b, (size_t)(r.name.e - r.name.b));
               ck_sb_puts(&m, ": takes ");
               put_span_repr(&m, &r.arg);
               ck_sb_puts(&m, " but has no parameters; a structured client"
                     " cannot call it");
               ck_strv_push(&bad, m.p);
            }
            else if (!takes_arg && !none)
            {
               ck_sb_add(&m, r.name.b, (size_t)(r.name.e - r.name.b));
               ck_sb_puts(&m, ": takes no argument but has parameters ");
               put_span_repr(&m, &r.params);
               ck_strv_push(&bad, m.p);
            }
         }
         p = r.end;
      }
   }
   if (bad.n)
   {
      printf("command_table_check: every command in %s needs a description"
            " and flags:\n", HEADER);
      for (i = 0; i < bad.n; i++)
         printf("  %s\n", bad.v[i]);
      return 1;
   }
   printf("ok:   %lu commands in %s all describe themselves\n", count, HEADER);
   free(text);
   return 0;
}

/* Thumbnail name matching (gfx/gfx_thumbnail_match.c).
 *
 *   gfx_thumbnail_match_test FIXTURES CASES
 *      the parser, fold and API checks, then every line of CASES
 *      (format at the top of cases.txt) against FIXTURES/<system>.index
 *   gfx_thumbnail_match_test --measure NAMES SYSTEM...
 *      leave-one-out over NAMES/<SYSTEM>.index; local only
 *   gfx_thumbnail_match_test --steps NAMES SYSTEM...
 *      the stepped finalize against qsort over NAMES/<SYSTEM>.index;
 *      local only */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../../gfx/gfx_thumbnail_match.c"

#define PATH_LEN 1024

struct names
{
   char **v;
   size_t count;
   size_t cap;
};

static const char *const tier_names[] = { "t0", "t2", "t2b", "t3", "t3b" };

static int failures = 0;

static bool names_push(struct names *l, const char *s, size_t n)
{
   char *copy;
   if (l->count == l->cap)
   {
      size_t cap = l->cap ? l->cap * 2 : 64;
      char **v   = (char**)realloc(l->v, cap * sizeof(*v));
      if (!v)
         return false;
      l->v   = v;
      l->cap = cap;
   }
   if (!(copy = (char*)malloc(n + 1)))
      return false;
   memcpy(copy, s, n);
   copy[n]          = '\0';
   l->v[l->count++] = copy;
   return true;
}

static void names_free(struct names *l)
{
   size_t i;
   for (i = 0; i < l->count; i++)
      free(l->v[i]);
   free(l->v);
   l->v     = NULL;
   l->count = 0;
   l->cap   = 0;
}

/* git's C-quoted path: "Named_Boxarts/Pok\303\251mon.png", \" \\ \t ... */
static size_t unquote(const char *s, size_t n, char *out)
{
   size_t i;
   size_t o = 0;
   if (n < 2 || s[0] != '"' || s[n - 1] != '"')
   {
      memcpy(out, s, n);
      return n;
   }
   for (i = 1; i < n - 1; i++)
   {
      unsigned c = (unsigned char)s[i];
      if (c == '\\' && i + 1 < n - 1)
      {
         c = (unsigned char)s[++i];
         if (c >= '0' && c <= '7' && i + 2 < n - 1)
         {
            c  = ((c - '0') << 6) | ((unsigned)(s[i + 1] - '0') << 3)
               | (unsigned)(s[i + 2] - '0');
            i += 2;
         }
         else if (c == 'n')
            c = '\n';
         else if (c == 't')
            c = '\t';
         else if (c == 'r')
            c = '\r';
         else if (c == 'a')
            c = '\a';
         else if (c == 'b')
            c = '\b';
         else if (c == 'f')
            c = '\f';
         else if (c == 'v')
            c = '\v';
      }
      out[o++] = (char)(c & 0xFF);
   }
   return o;
}

/* The names in one folder (type) of a `git ls-tree -r --name-only` list. */
static bool parse_index(const char *buf, size_t len, const char *type,
      struct names *out)
{
   size_t tlen   = strlen(type);
   const char *p = buf;
   const char *e = buf + len;
   char *line    = (char*)malloc(len + 1);
   bool ok       = (line != NULL);
   while (ok && p < e)
   {
      const char *nl = (const char*)memchr(p, '\n', (size_t)(e - p));
      const char *le = nl ? nl : e;
      size_t n;
      if (le > p && le[-1] == '\r')
         le--;
      n = unquote(p, (size_t)(le - p), line);
      if (     n > tlen + 1 && !memcmp(line, type, tlen) && line[tlen] == '/'
            && !memchr(line + tlen + 1, '/', n - tlen - 1))
         ok = names_push(out, line + tlen + 1, n - tlen - 1);
      p = nl ? nl + 1 : e;
   }
   free(line);
   return ok;
}

static char *load_file(const char *path, size_t *len)
{
   FILE *f   = fopen(path, "rb");
   char *buf = NULL;
   long size = 0;
   if (!f)
      return NULL;
   if (     fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) >= 0
         && fseek(f, 0, SEEK_SET) == 0
         && (buf = (char*)malloc((size_t)size + 1)))
   {
      *len      = fread(buf, 1, (size_t)size, f);
      buf[*len] = '\0';
   }
   fclose(f);
   return buf;
}

static bool load_names(const char *dir, const char *system, struct names *l)
{
   char path[PATH_LEN];
   char *buf;
   size_t len = 0;
   bool ok;
   if (strlen(dir) + strlen(system) + 8 > sizeof(path))
      return false;
   strcpy(path, dir);
   strcat(path, "/");
   strcat(path, system);
   strcat(path, ".index");
   if (!(buf = load_file(path, &len)))
      return false;
   ok = parse_index(buf, len, "Named_Boxarts", l);
   free(buf);
   return ok;
}

static void test_parser(void)
{
   static const char text[] =
      ".gitignore\n"
      "Named_Boxarts/Tetris (USA).png\r\n"
      "\"Named_Boxarts/Pok\\303\\251mon - Red Version (USA, Europe) (SGB Enhanced).png\"\n"
      "\"Named_Boxarts/Say \\\"Hi\\\" \\\\ Bye.png\"\n"
      "Named_Snaps/Tetris (USA).png\n"
      "\"Named_Boxarts/Named_Boxarts/Pop\\342\\200\\231n TwinBee (USA).png\"\n"
      "Named_Boxarts/Last (Japan).png";
   static const char *const want[] = {
      "Tetris (USA).png",
      "Pok\303\251mon - Red Version (USA, Europe) (SGB Enhanced).png",
      "Say \"Hi\" \\ Bye.png",
      "Last (Japan).png" };
   struct names l;
   size_t i;
   memset(&l, 0, sizeof(l));
   if (!parse_index(text, sizeof(text) - 1, "Named_Boxarts", &l))
   {
      printf("[FAIL] index parser: out of memory\n");
      failures++;
   }
   else if (l.count != GTM_COUNT(want))
   {
      printf("[FAIL] index parser: %u names, want %u\n",
            (unsigned)l.count, (unsigned)GTM_COUNT(want));
      failures++;
   }
   else
      for (i = 0; i < l.count; i++)
         if (strcmp(l.v[i], want[i]))
         {
            printf("[FAIL] index parser: got \"%s\", want \"%s\"\n",
                  l.v[i], want[i]);
            failures++;
         }
   names_free(&l);
}

static void test_fold(void)
{
   static const char *const cases[][2] = {
      { "Legend of Zelda, The - Ocarina of Time (USA)",
        "the legend of zelda ocarina of time usa" },
      { "Zelda, The", "the zelda" },
      { "Pok\303\251mon - Red Version", "pokemon red version" },
      { "Stra\303\237e", "strasse" },
      { "\303\206gis", "aegis" },
      { "Bump 'n' Jump (USA)", "bump n jump usa" },
      { "Pop\342\200\231n TwinBee", "popn twinbee" },
      { "Mr. Do!", "mr do" },
      { "Advanced Dungeons _ Dragons", "advanced dungeons dragons" },
      { "Itchy & Scratchy Game, The (USA)", "the itchy scratchy game usa" },
      { "Itchy _ Scratchy Game, The (USA)", "the itchy scratchy game usa" },
      { "Ren & Stimpy Show, The - Fire Dogs (USA)",
        "the ren stimpy show fire dogs usa" },
      { "Ren _ Stimpy Show, The - Fire Dogs (USA)",
        "the ren stimpy show fire dogs usa" },
      { "Duel, The: Test Drive II (Accolade Inc)",
        "the duel test drive ii accolade inc" },
      { "Duel, The_ Test Drive II (Accolade Inc)",
        "the duel test drive ii accolade inc" },
      { "Tetris\302\262 (USA)", "tetris2 usa" },
      { "Tetris2 (USA)", "tetris2 usa" },
      { "\302\271x\302\263", "1x3" },
      { "Ranma \302\275", "ranma 1 2" },
      { "Ranma\302\275 (Japan)", "ranma 1 2 japan" },
      { "\302\274\302\276", "1 4 3 4" },
      { "  Spaced   Out  ", "spaced out" },
      { "\343\203\206\343\203\210\343\203\252\343\202\271 (Japan)",
        "\343\203\206\343\203\210\343\203\252\343\202\271 japan" },
      { "Mega Man X", "mega man x" },
      { "Final Fantasy II", "final fantasy ii" },
      { "Super Mario Bros. 3 (USA) (Rev 1)", "super mario bros 3 usa rev 1" },
      { "", "" } };
   char out[64];
   size_t i;
   for (i = 0; i < GTM_COUNT(cases); i++)
   {
      size_t n = gfx_thumbnail_match_fold(cases[i][0], out, sizeof(out));
      if (strcmp(out, cases[i][1]) || n != strlen(cases[i][1]))
      {
         printf("[FAIL] fold \"%s\": got \"%s\", want \"%s\"\n",
               cases[i][0], out, cases[i][1]);
         failures++;
      }
   }
   if (     gfx_thumbnail_match_fold("Tetris (USA)", out, 5) != 4
         || strcmp(out, "tetr"))
   {
      printf("[FAIL] fold into 5 bytes: got \"%s\"\n", out);
      failures++;
   }
   if (gfx_thumbnail_match_fold(NULL, out, sizeof(out)) || out[0])
   {
      printf("[FAIL] fold of NULL is not empty\n");
      failures++;
   }
}

static void expect(const gfx_thumbnail_match_index_t *idx,
      const char *const *queries, size_t count,
      enum gfx_thumbnail_match_status want, const char *want_name)
{
   const char *out  = "unset";
   const char *last = (count && queries[count - 1]) ? queries[count - 1] : "-";
   enum gfx_thumbnail_match_status st =
      gfx_thumbnail_match_find_first(idx, queries, count, &out);
   if (st != want || (want_name ? !out || strcmp(out, want_name) : out != NULL))
   {
      printf("[FAIL] find_first up to \"%s\": status %d (%s), want %d (%s)\n",
            last, (int)st, out ? out : "null", (int)want,
            want_name ? want_name : "null");
      failures++;
   }
}

static void test_api(void)
{
   static const char *const names[] = {
      "Tetris (USA).png", "Tetris (USA) (Tengen) (Unl).png", "readme.txt",
      ".png", "Fight, The (Japan).png", "Fight (Japan).png" };
   const char *labels[3];
   const char *refused[2];
   const char *first  = NULL;
   const char *second = NULL;
   gfx_thumbnail_match_index_t *idx = gfx_thumbnail_match_index_new();
   size_t i;
   gfx_thumbnail_match_index_free(NULL);
   gfx_thumbnail_match_index_finalize(NULL);
   if (     gfx_thumbnail_match_index_add(NULL, "a.png")
         || gfx_thumbnail_match_find(NULL, "a", &first)
            != GFX_THUMBNAIL_MATCH_NONE || first)
   {
      printf("[FAIL] NULL index is not refused\n");
      failures++;
   }
   for (i = 0; idx && i < GTM_COUNT(names); i++)
      if (!gfx_thumbnail_match_index_add(idx, names[i]))
      {
         printf("[FAIL] add \"%s\" failed\n", names[i]);
         failures++;
      }
   if (gfx_thumbnail_match_find(idx, "Tetris (USA)", &first)
         != GFX_THUMBNAIL_MATCH_NONE)
   {
      printf("[FAIL] find before finalize\n");
      failures++;
   }
   gfx_thumbnail_match_index_finalize(idx);
   if (gfx_thumbnail_match_index_add(idx, "Late (USA).png"))
   {
      printf("[FAIL] add after finalize is accepted\n");
      failures++;
   }
   labels[0] = NULL;
   labels[1] = "\343\203\206\343\203\210\343\203\252\343\202\271";
   labels[2] = "Tetris (USA)";
   expect(idx, labels, 3, GFX_THUMBNAIL_MATCH_FOUND, "Tetris (USA).png");
   expect(idx, labels, 2, GFX_THUMBNAIL_MATCH_NONE, NULL);
   expect(idx, labels, 0, GFX_THUMBNAIL_MATCH_NONE, NULL);
   labels[1] = "readme";
   expect(idx, labels, 2, GFX_THUMBNAIL_MATCH_NONE, NULL);
   refused[0] = "A Fight (Japan)";
   refused[1] = "Fight (Japan)";
   expect(idx, refused, 1, GFX_THUMBNAIL_MATCH_REFUSED, NULL);
   expect(idx, refused, 2, GFX_THUMBNAIL_MATCH_FOUND, "Fight (Japan).png");
   gfx_thumbnail_match_find(idx, "Tetris (USA)", &first);
   gfx_thumbnail_match_find(idx, "Fight (Japan)", &second);
   gfx_thumbnail_match_find(idx, "TETRIS (USA)", &second);
   if (!first || first != second || strcmp(first, "Tetris (USA).png"))
   {
      printf("[FAIL] a found name moved or changed\n");
      failures++;
   }
   gfx_thumbnail_match_index_free(idx);
   idx = gfx_thumbnail_match_index_new();
   gfx_thumbnail_match_index_finalize(idx);
   labels[2] = "Tetris (USA)";
   expect(idx, labels, 3, GFX_THUMBNAIL_MATCH_NONE, NULL);
   gfx_thumbnail_match_index_free(idx);
}

/* f: system, query, expected, tier, then +add and -hide names. */
static void run_case(const char *dir, char **f, size_t nf)
{
   struct names base;
   gfx_thumbnail_match_index_t *idx;
   const struct gtm_rec *hit = NULL;
   enum gfx_thumbnail_match_status st;
   const char *got;
   const char *got_tier;
   unsigned tier = 0;
   bool added    = true;
   size_t i, j;
   memset(&base, 0, sizeof(base));
   for (j = 4; j < nf; j++)
      if ((f[j][0] != '+' && f[j][0] != '-') || !f[j][1])
      {
         printf("[FAIL] %s: bad name field \"%s\"\n", f[1], f[j]);
         failures++;
         return;
      }
   if (strcmp(f[0], "-") && !load_names(dir, f[0], &base))
   {
      printf("[FAIL] %s: cannot read %s/%s.index\n", f[1], dir, f[0]);
      failures++;
      return;
   }
   for (j = 4; j < nf; j++)
   {
      bool used = (f[j][0] == '+');
      for (i = 0; !used && i < base.count; i++)
         used = !strcmp(f[j] + 1, base.v[i]);
      if (!used)
      {
         printf("[FAIL] %s: %s hides no name\n", f[1], f[j]);
         failures++;
      }
   }
   if (!(idx = gfx_thumbnail_match_index_new()))
   {
      printf("[FAIL] %s: index_new failed\n", f[1]);
      failures++;
      names_free(&base);
      return;
   }
   for (i = 0; i < base.count; i++)
   {
      bool hidden = false;
      for (j = 4; j < nf; j++)
         if (f[j][0] == '-' && !strcmp(f[j] + 1, base.v[i]))
            hidden = true;
      if (!hidden && !gfx_thumbnail_match_index_add(idx, base.v[i]))
         added = false;
   }
   for (j = 4; j < nf; j++)
      if (f[j][0] == '+' && !gfx_thumbnail_match_index_add(idx, f[j] + 1))
         added = false;
   if (!added)
   {
      printf("[FAIL] %s: index_add failed\n", f[1]);
      failures++;
      gfx_thumbnail_match_index_free(idx);
      names_free(&base);
      return;
   }
   gfx_thumbnail_match_index_finalize(idx);
   st       = gtm_find(idx, f[1], &hit, &tier, NULL);
   got      = hit ? hit->name : (st == GFX_THUMBNAIL_MATCH_REFUSED) ? "!" : "-";
   got_tier = (st == GFX_THUMBNAIL_MATCH_NONE) ? "-" : tier_names[tier];
   if (strcmp(got, f[2]) || strcmp(got_tier, f[3]))
   {
      printf("[FAIL] %s|%s: got %s (%s), want %s (%s)\n",
            f[0], f[1], got, got_tier, f[2], f[3]);
      failures++;
   }
   gfx_thumbnail_match_index_free(idx);
   names_free(&base);
}

static void run_cases(const char *dir, const char *path)
{
   size_t len   = 0;
   unsigned run = 0;
   char *text   = load_file(path, &len);
   char *line;
   char *next;
   if (!text)
   {
      printf("[FAIL] cannot read %s\n", path);
      failures++;
      return;
   }
   for (line = text; line && *line; line = next)
   {
      char *f[64];
      size_t nf = 0;
      char *p;
      if ((next = strchr(line, '\n')))
         *next++ = '\0';
      if ((p = strchr(line, '\r')))
         *p = '\0';
      if (!*line || *line == '#')
         continue;
      f[nf++] = line;
      for (p = line; nf < GTM_COUNT(f) && (p = strchr(p, '|')); )
      {
         *p++    = '\0';
         f[nf++] = p;
      }
      if (nf < 4)
      {
         printf("[FAIL] malformed case: %s\n", line);
         failures++;
         continue;
      }
      run_case(dir, f, nf);
      run++;
   }
   printf("%u cases\n", run);
   free(text);
}

/* Digit runs and roman numerals II-XX of a title key, in order: "F1" and
 * "F 1" both read 1; a lone I, V or X is a word, as in T2b. */
static size_t numerals(const char *key, size_t n, unsigned *v, size_t max)
{
   static const char *const romans[] = {
      "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x", "xi",
      "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx" };
   size_t count = 0;
   size_t i     = 0;
   while (i < n && count < max)
   {
      size_t start = i;
      size_t k;
      while (i < n && key[i] != ' ')
         i++;
      for (k = start; k < i && count < max; )
      {
         size_t d = gtm_span_digits(key + k, i - k);
         if (d)
            v[count++] = gtm_number(key + k, d);
         k += d ? d : 1;
      }
      for (k = 0; k < GTM_COUNT(romans) && count < max; k++)
         if (     i - start > 1 && strlen(romans[k]) == i - start
               && !memcmp(romans[k], key + start, i - start))
            v[count++] = (unsigned)k + 1;
      i++;
   }
   return count;
}

static bool same_numerals(const struct gtm_rec *a, const struct gtm_rec *b)
{
   unsigned va[16], vb[16];
   size_t na = numerals(a->key, a->tlen, va, 16);
   size_t nb = numerals(b->key, b->tlen, vb, 16);
   return na == nb && !memcmp(va, vb, na * sizeof(*va));
}

static void print_pair(const char *label, const struct gtm_rec *a,
      const struct gtm_rec *b)
{
   printf("   %-8s %.*s -> %s\n", label, (int)a->stem_len, a->name, b->name);
}

static int measure(const char *dir, int count, char **systems)
{
   int s;
   int bad = 0;
   for (s = 0; s < count; s++)
   {
      struct names l;
      gfx_thumbnail_match_index_t *idx;
      unsigned long hits[5];
      unsigned long none    = 0;
      unsigned long refused = 0;
      unsigned long numeral = 0;
      unsigned t2b_shown    = 0;
      unsigned region_shown = 0;
      size_t i;
      memset(&l, 0, sizeof(l));
      memset(hits, 0, sizeof(hits));
      if (!load_names(dir, systems[s], &l)
            || !(idx = gfx_thumbnail_match_index_new()))
      {
         printf("cannot read %s/%s.index\n", dir, systems[s]);
         names_free(&l);
         bad = 1;
         continue;
      }
      for (i = 0; i < l.count; i++)
         gfx_thumbnail_match_index_add(idx, l.v[i]);
      gfx_thumbnail_match_index_finalize(idx);
      printf("== %s: %u names\n", systems[s], (unsigned)idx->count);
      for (i = 0; i < idx->count; i++)
      {
         const struct gtm_rec *r   = &idx->recs[i];
         const struct gtm_rec *hit = NULL;
         char stem[GTM_NAME_MAX];
         unsigned tier = 0;
         enum gfx_thumbnail_match_status st;
         memcpy(stem, r->name, r->stem_len);
         stem[r->stem_len] = '\0';
         st = gtm_find(idx, stem, &hit, &tier, r);
         if (st == GFX_THUMBNAIL_MATCH_NONE)
            none++;
         else if (st == GFX_THUMBNAIL_MATCH_REFUSED)
            refused++;
         else
         {
            hits[tier]++;
            if (!same_numerals(r, hit))
            {
               print_pair("NUMERAL", r, hit);
               numeral++;
            }
            else if (tier == GTM_T2B && t2b_shown < 20)
            {
               print_pair("t2b", r, hit);
               t2b_shown++;
            }
            else if (   (tier == GTM_T3 || tier == GTM_T3B)
                     && hit->region != r->region && region_shown < 20)
            {
               print_pair("region", r, hit);
               region_shown++;
            }
         }
      }
      printf("   t0 %lu  t2 %lu  t2b %lu  t3 %lu  t3b %lu  none %lu  "
            "refused %lu  numeral failures %lu\n", hits[GTM_T0], hits[GTM_T2],
            hits[GTM_T2B], hits[GTM_T3], hits[GTM_T3B], none, refused, numeral);
      if (numeral)
         bad = 1;
      gfx_thumbnail_match_index_free(idx);
      names_free(&l);
   }
   return bad;
}

struct sort_el
{
   unsigned key;
   unsigned seq;
};

static int sort_el_cmp_key(const void *pa, const void *pb)
{
   const struct sort_el *a = (const struct sort_el*)pa;
   const struct sort_el *b = (const struct sort_el*)pb;
   return (a->key > b->key) - (a->key < b->key);
}

static int sort_el_cmp_stable(const void *pa, const void *pb)
{
   const struct sort_el *a = (const struct sort_el*)pa;
   const struct sort_el *b = (const struct sort_el*)pb;
   int r                   = sort_el_cmp_key(pa, pb);
   return r ? r : (a->seq > b->seq) - (a->seq < b->seq);
}

/* Moves of the step just taken; a new _begin resets the count. */
static size_t step_moves(const gfx_thumbnail_match_sort_t *s,
      const char *base, size_t moved)
{
   return (s->base == base) ? s->moved - moved : s->moved;
}

/* Stable on equal keys, bounded per step, any length. */
static void test_sort(void)
{
   static const size_t lens[] = { 0, 1, 2, 3, 7,
      GFX_THUMBNAIL_MATCH_SORT_STEP - 1, GFX_THUMBNAIL_MATCH_SORT_STEP,
      GFX_THUMBNAIL_MATCH_SORT_STEP + 1, 2 * GFX_THUMBNAIL_MATCH_SORT_STEP + 3,
      5 * GFX_THUMBNAIL_MATCH_SORT_STEP + 1 };
   size_t n;
   for (n = 0; n < GTM_COUNT(lens); n++)
   {
      gfx_thumbnail_match_sort_t s;
      size_t len           = lens[n];
      size_t most          = 0;
      unsigned long v      = 12345;
      struct sort_el *a    = (struct sort_el*)malloc((len + 1) * sizeof(*a));
      struct sort_el *ref  = (struct sort_el*)malloc((len + 1) * sizeof(*a));
      struct sort_el *tmp  = (struct sort_el*)malloc((len + 1) * sizeof(*a));
      size_t i;
      if (!a || !ref || !tmp)
      {
         printf("[FAIL] sort %u: OOM\n", (unsigned)len);
         failures++;
         free(a);
         free(ref);
         free(tmp);
         continue;
      }
      for (i = 0; i < len; i++)
      {
         v        = (v * 1103515245UL + 12345UL) & 0x7fffffffUL;
         a[i].key = (unsigned)(v % (len / 3 + 1));
         a[i].seq = (unsigned)i;
      }
      memcpy(ref, a, len * sizeof(*a));
      qsort(ref, len, sizeof(*ref), sort_el_cmp_stable);
      gfx_thumbnail_match_sort_begin(&s, a, tmp, len, sizeof(*a),
            sort_el_cmp_key);
      for (;;)
      {
         size_t moved = s.moved;
         bool more    = gfx_thumbnail_match_sort_step(&s);
         size_t m     = s.moved - moved;
         if (m > most)
            most = m;
         if (!more)
         {
            if (m)
            {
               printf("[FAIL] sort %u: the last step moved %u\n",
                     (unsigned)len, (unsigned)m);
               failures++;
            }
            break;
         }
      }
      if (most > GFX_THUMBNAIL_MATCH_SORT_STEP)
      {
         printf("[FAIL] sort %u: a step moved %u\n", (unsigned)len,
               (unsigned)most);
         failures++;
      }
      if (len && memcmp(a, ref, len * sizeof(*a)))
      {
         printf("[FAIL] sort %u: not qsort's stable order\n", (unsigned)len);
         failures++;
      }
      if (gfx_thumbnail_match_sort_step(&s))
      {
         printf("[FAIL] sort %u: a step after the end\n", (unsigned)len);
         failures++;
      }
      free(a);
      free(ref);
      free(tmp);
   }
}

/* Runs the stepped finalize on idx, unfinalized, and compares both orders
 * with qsort's. Returns false on a failure. */
static bool check_steps(gfx_thumbnail_match_index_t *idx, const char *what,
      bool timed)
{
   struct gtm_rec *ref;
   const struct gtm_rec **ref_loose;
   char stem[GTM_NAME_MAX];
   const char *found = NULL;
   size_t count      = idx->count;
   size_t steps      = 0;
   size_t most       = 0;
   size_t dups       = 0;
   clock_t slow      = 0;
   clock_t all       = 0;
   bool ok           = true;
   size_t i;
   if (!(ref = (struct gtm_rec*)malloc((count + 1) * sizeof(*ref))))
      return false;
   memcpy(ref, idx->recs, count * sizeof(*ref));
   qsort(ref, count, sizeof(*ref), gtm_cmp_title);
   for (i = 1; i < count; i++)
      if (!strcmp(ref[i - 1].name, ref[i].name))
         dups++;
   stem[0] = '\0';
   if (count)
   {
      memcpy(stem, ref[0].name, ref[0].stem_len);
      stem[ref[0].stem_len] = '\0';
   }
   for (;;)
   {
      const char *base = idx->sort.base;
      size_t moved     = idx->sort.moved;
      clock_t t0       = clock();
      bool more        = gfx_thumbnail_match_index_finalize_step(idx);
      clock_t t        = clock() - t0;
      size_t m         = step_moves(&idx->sort, base, moved);
      all             += t;
      if (t > slow)
         slow = t;
      if (m > most)
         most = m;
      if (!more)
         break;
      steps++;
      if (steps == 1 && count)
      {
         const char *out = NULL;
         if (     gfx_thumbnail_match_index_add(idx, "Late (USA).png")
               || gfx_thumbnail_match_find(idx, stem, &out)
                  != GFX_THUMBNAIL_MATCH_NONE || out)
         {
            printf("[FAIL] %s: add or find during finalize\n", what);
            ok = false;
         }
      }
   }
   if (most > GFX_THUMBNAIL_MATCH_SORT_STEP)
   {
      printf("[FAIL] %s: a step moved %u\n", what, (unsigned)most);
      ok = false;
   }
   if (count && gfx_thumbnail_match_find(idx, stem, &found)
         != GFX_THUMBNAIL_MATCH_FOUND)
   {
      printf("[FAIL] %s: no find once finalized\n", what);
      ok = false;
   }
   if (!idx->finalized || idx->scratch || idx->count != count)
   {
      printf("[FAIL] %s: not finalized\n", what);
      ok = false;
   }
   else if (count && memcmp(idx->recs, ref, count * sizeof(*ref)))
   {
      printf("[FAIL] %s: title order differs from qsort\n", what);
      ok = false;
   }
   else if (count && (ref_loose = (const struct gtm_rec**)malloc(
         count * sizeof(*ref_loose))))
   {
      for (i = 0; i < count; i++)
         ref_loose[i] = &idx->recs[i];
      qsort(ref_loose, count, sizeof(*ref_loose), gtm_cmp_loose);
      if (memcmp(idx->by_loose, ref_loose, count * sizeof(*ref_loose)))
      {
         printf("[FAIL] %s: loose order differs from qsort\n", what);
         ok = false;
      }
      free(ref_loose);
   }
   if (timed)
      printf("== %s: %u names, %u duplicate names, %u steps, at most %u "
            "moves and %.3f ms a step, %.1f ms in all: %s\n", what,
            (unsigned)count, (unsigned)dups, (unsigned)steps + 1,
            (unsigned)most, slow * 1000.0 / CLOCKS_PER_SEC,
            all * 1000.0 / CLOCKS_PER_SEC, ok ? "same as qsort" : "FAILED");
   free(ref);
   return ok;
}

/* Many names per title, in no order: every tie-break is reached. */
static void test_finalize_steps(void)
{
   static const char *const titles[] = { "Tetris", "TETRIS", "Tetris 2",
      "Fight, The", "The Fight", "Fight", "Game & Watch", "Game and Watch",
      "Super Game", "Super Game II", "Super Game 2", "Pac-Man", "Pac Man",
      "Ys", "Ys I & II", "Zelda" };
   static const char *const regions[] = { "USA", "Europe", "Japan", "World",
      "USA, Europe", "Japan, USA", "Germany", "France" };
   gfx_thumbnail_match_index_t *idx = gfx_thumbnail_match_index_new();
   size_t n                         = 2 * GFX_THUMBNAIL_MATCH_SORT_STEP + 1001;
   size_t i;
   for (i = 0; idx && i < n; i++)
   {
      char name[64];
      unsigned v = (unsigned)((i * 7919) % 16384);
      sprintf(name, "%s (%s) (Rev %u).%s", titles[v % 16],
            regions[(v / 16) % 8], v / 128, (v % 5) ? "png" : "jpg");
      if (!gfx_thumbnail_match_index_add(idx, name))
         break;
   }
   if (!idx || idx->count != n || !check_steps(idx, "stepped finalize", false))
   {
      printf("[FAIL] stepped finalize\n");
      failures++;
   }
   gfx_thumbnail_match_index_free(idx);
}

static int steps_lists(const char *dir, int count, char **systems)
{
   int s;
   int bad = 0;
   for (s = 0; s < count; s++)
   {
      struct names l;
      gfx_thumbnail_match_index_t *idx = NULL;
      size_t i;
      memset(&l, 0, sizeof(l));
      if (     !load_names(dir, systems[s], &l)
            || !(idx = gfx_thumbnail_match_index_new()))
      {
         printf("cannot read %s/%s.index\n", dir, systems[s]);
         bad = 1;
      }
      for (i = 0; idx && i < l.count; i++)
         gfx_thumbnail_match_index_add(idx, l.v[i]);
      if (idx && !check_steps(idx, systems[s], true))
         bad = 1;
      gfx_thumbnail_match_index_free(idx);
      names_free(&l);
   }
   return bad;
}

int main(int argc, char **argv)
{
   if (argc >= 4 && !strcmp(argv[1], "--measure"))
      return measure(argv[2], argc - 3, argv + 3);
   if (argc >= 4 && !strcmp(argv[1], "--steps"))
      return steps_lists(argv[2], argc - 3, argv + 3);
   if (argc != 3)
   {
      printf("usage: %s FIXTURES CASES\n"
             "       %s --measure NAMES SYSTEM...\n"
             "       %s --steps NAMES SYSTEM...\n", argv[0], argv[0], argv[0]);
      return 2;
   }
   test_parser();
   test_fold();
   test_api();
   test_sort();
   test_finalize_steps();
   run_cases(argv[1], argv[2]);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] gfx_thumbnail_match_test\n");
   return 0;
}

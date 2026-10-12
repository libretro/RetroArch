/* Packed translation table consistency check.
 *
 * Usage, from the repository root:
 *     tools/check.sh msg_hash_packed_check [intl/msg_hash_xx.h ...]
 *     tools/check.sh msg_hash_packed_check --selftest
 *
 * Every intl/msg_hash_<lang>.h that intl/json2h.py emits carries two
 * objects paired by position and by nothing else: the blob struct holds
 * the rows as one run of NUL-terminated strings, and ids[] names the enum
 * each row belongs to. msg_hash_strtab_index_build() walks the blob once
 * and gives row N to ids[N], so a row present on one side and absent on
 * the other hands every row after it to the wrong enum, and the menu
 * fills with real strings belonging to other entries. A C compiler cannot
 * see that: both objects are well formed on their own, and the sizeof()
 * contiguity check in the header measures the blob against itself.
 *
 * The pairing is recoverable because json2h.py names each struct member
 * after the djb2 hash of the key its row was emitted for, so this checks
 * it directly: row N's member name must be the name the key in ids[N]
 * hashes to, under the same preprocessor guards. The row sizes the walk
 * depends on are checked with it - a member one byte wider than its
 * string leaves a stray NUL that ends the walk's row early and shifts
 * everything after it just as surely.
 *
 * Both checks read the whole file and follow the guards, so this sees
 * configurations no single build job compiles.
 *
 * intl/json2h.py runs this on every header it writes before writing it. */

#include "msg_hash_common.h"

struct row
{
   char *base;
   char *guard;
   unsigned long size;
   int open;
};

static void error(struct ck_strv *errors, struct ck_sb *msg)
{
   ck_strv_push(errors, msg->p);
   msg->p   = NULL;
   msg->len = msg->cap = 0;
}

/* The name of the row a member belongs to: the chunk suffix json2h.py
 * appends to a split row taken off.
 * ^s_[0-9a-f]{8}(?:_c\d+)?_\d+$ */
static char *row_name(const char *m)
{
   const char *p = m + 2;
   const char *tail;
   int i;
   if (strncmp(m, "s_", 2))
      return ck_strdup(m);
   for (i = 0; i < 8; i++, p++)
   {
      if (!(CK_IS_DIGIT(*p) || (*p >= 'a' && *p <= 'f')))
         return ck_strdup(m);
   }
   for (i = 0; i < 2; i++)
   {
      const char *q = p;
      if (i == 0)
      {
         if (q[0] != '_' || q[1] != 'c' || !CK_IS_DIGIT(q[2]))
            continue;
         for (q += 2; CK_IS_DIGIT(*q); q++);
      }
      if (*q != '_' || !CK_IS_DIGIT(q[1]))
         continue;
      for (tail = ++q; CK_IS_DIGIT(*q); q++);
      if (!*q)
         return ck_strndup(m, (size_t)(tail - 1 - m));
   }
   return ck_strdup(m);
}

/* \s*\(sizeof\(msg_hash_\w+_blob\) == \((\d+)u  or  \s*\+ (\d+)u
 * Returns 0 when the line is not one. */
static int size_term(const char *b, const char *e, int preamble,
      unsigned long *v)
{
   const char *p = mh_skip_space(b, e);
   const char *w, *d;
   if (preamble)
   {
      if (!mh_starts(p, e, "(sizeof(msg_hash_"))
         return 0;
      for (w = p += 17; p < e && CK_IS_WORD(*p); p++);
      if (p - w < 6 || memcmp(p - 5, "_blob", 5)
            || !mh_starts(p, e, ") == ("))
         return 0;
      p += 6;
   }
   else
   {
      if (!mh_starts(p, e, "+ "))
         return 0;
      p += 2;
   }
   for (d = p; p < e && CK_IS_DIGIT(*p); p++);
   if (p == d || p >= e || *p != 'u')
      return 0;
   *v = strtoul(d, NULL, 10);
   return 1;
}

static void add_to(struct ck_set *sums, const char *guard, unsigned long v)
{
   void **slot = ck_set_add(sums, guard, strlen(guard));
   if (!*slot)
   {
      *slot = ck_alloc(sizeof(unsigned long));
      *(unsigned long*)*slot = 0;
   }
   *(unsigned long*)*slot += v;
}

static unsigned long sum_of(const struct ck_set *sums, const char *guard)
{
   void **slot = ck_set_find(sums, guard, strlen(guard));
   return slot && *slot ? *(unsigned long*)*slot : 0;
}

struct guard_key
{
   char *repr;
   const char *guard;
};

static int cmp_key(const void *a, const void *b)
{
   return strcmp(((const struct guard_key*)a)->repr,
         ((const struct guard_key*)b)->repr);
}

static void put_guard(struct ck_sb *sb, const char *guard)
{
   mh_repr_guard(sb, guard);
}

static void free_set(struct ck_set *set)
{
   size_t i;
   for (i = 0; i < set->cap; i++)
   {
      free(set->keys[i]);
      free(set->vals[i]);
   }
   free(set->keys);
   free(set->vals);
   memset(set, 0, sizeof(*set));
}

/* One packed header's text checked; the errors go into errors. */
static void check_text(const char *text, size_t len, const char *lang,
      const char *name, struct ck_strv *errors)
{
   struct mh_lines sec[4];
   struct mh_lines ml, il, dl, cl;
   struct ck_strv mnames = {0};
   unsigned long *msizes;
   char **mguards;
   struct ck_strv inits  = {0};
   struct ck_strv iguard = {0};
   size_t *ilens         = NULL;
   struct ck_strv ids    = {0};
   struct ck_strv idg    = {0};
   struct row *rows      = NULL;
   size_t nrows          = 0;
   struct ck_sb msg      = {0};
   struct ck_sb cur      = {0};
   int have_cur          = 0;
   char *cur_guard       = NULL;
   size_t i;

   if (!mh_is_packed(text, len, lang))
      return;
   if (!mh_split_sections(text, len, lang, sec))
   {
      ck_sb_puts(&msg, name);
      ck_sb_puts(&msg, ": generated regions not found");
      error(errors, &msg);
      return;
   }

   mh_scan_guarded(&sec[0], 0, &ml);
   msizes  = (unsigned long*)ck_alloc((ml.n + 1) * sizeof(*msizes));
   mguards = (char**)ck_alloc((ml.n + 1) * sizeof(*mguards));
   for (i = 0; i < ml.n; i++)
   {
      char *mname;
      unsigned long size;
      if (!mh_member(ml.v[i].b, ml.v[i].e, &mname, &size))
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": unparsed struct member: ");
         ck_sb_add(&msg, ml.v[i].b, (size_t)(ml.v[i].e - ml.v[i].b));
         error(errors, &msg);
         continue;
      }
      msizes[mnames.n]  = size;
      mguards[mnames.n] = ml.v[i].guard;
      ck_strv_push(&mnames, mname);
   }

   mh_scan_guarded(&sec[1], 0, &il);
   for (i = 0; i < il.n; i++)
   {
      if (!have_cur)
      {
         have_cur  = 1;
         cur.len   = 0;
         ck_sb_puts(&cur, "");
         cur_guard = il.v[i].guard;
      }
      mh_line_literals(il.v[i].b, il.v[i].e, &cur);
      if (mh_rstrip_ends_with(il.v[i].b, il.v[i].e, ','))
      {
         ilens = (size_t*)ck_realloc(ilens, (inits.n + 1) * sizeof(*ilens));
         ilens[inits.n] = cur.len;
         ck_strv_push(&inits, ck_strndup(cur.p, cur.len));
         ck_strv_push(&iguard, ck_strdup(cur_guard));
         have_cur = 0;
      }
   }
   if (have_cur)
   {
      ck_sb_puts(&msg, name);
      ck_sb_puts(&msg, ": unterminated initializer");
      error(errors, &msg);
   }

   mh_scan_guarded(&sec[3], 0, &dl);
   for (i = 0; i < dl.n; i++)
   {
      char *id = mh_ident(dl.v[i].b, dl.v[i].e);
      if (!id)
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": unparsed ids entry: ");
         ck_sb_add(&msg, dl.v[i].b, (size_t)(dl.v[i].e - dl.v[i].b));
         error(errors, &msg);
         continue;
      }
      ck_strv_push(&ids, id);
      ck_strv_push(&idg, ck_strdup(dl.v[i].guard));
   }

   if (mnames.n != inits.n)
   {
      ck_sb_puts(&msg, name);
      ck_sb_puts(&msg, ": ");
      ck_sb_putu(&msg, (unsigned long)mnames.n);
      ck_sb_puts(&msg, " struct members but ");
      ck_sb_putu(&msg, (unsigned long)inits.n);
      ck_sb_puts(&msg, " initializers");
      error(errors, &msg);
      goto done;
   }

   /* Each member is exactly its decoded row plus the NUL, or exactly
    * the chunk for a non-final chunk of a split row. Anything wider
    * leaves a NUL the walk stops on; anything narrower runs two rows
    * together. */
   rows = (struct row*)ck_alloc((mnames.n + 1) * sizeof(*rows));
   for (i = 0; i < mnames.n; i++)
   {
      unsigned long size = msizes[i];
      size_t dlen        = ilens[i];
      int last           = size == (unsigned long)dlen + 1;
      char *base;
      if (strcmp(mguards[i], iguard.v[i]))
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": member ");
         ck_sb_puts(&msg, mnames.v[i]);
         ck_sb_puts(&msg, " is guarded ");
         put_guard(&msg, mguards[i]);
         ck_sb_puts(&msg, " and its initializer ");
         put_guard(&msg, iguard.v[i]);
         error(errors, &msg);
      }
      if (memchr(inits.v[i], '\0', dlen))
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": member ");
         ck_sb_puts(&msg, mnames.v[i]);
         ck_sb_puts(&msg, " holds an embedded NUL");
         error(errors, &msg);
      }
      if (!last && size != (unsigned long)dlen)
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": member ");
         ck_sb_puts(&msg, mnames.v[i]);
         ck_sb_puts(&msg, " declared [");
         ck_sb_putu(&msg, size);
         ck_sb_puts(&msg, "] holds ");
         ck_sb_putu(&msg, (unsigned long)dlen);
         ck_sb_puts(&msg, " bytes");
         error(errors, &msg);
      }
      base = row_name(mnames.v[i]);
      if (nrows && rows[nrows - 1].open && !strcmp(rows[nrows - 1].base, base))
      {
         rows[nrows - 1].size += size;
         rows[nrows - 1].open  = !last;
         free(base);
      }
      else
      {
         if (nrows && rows[nrows - 1].open)
         {
            ck_sb_puts(&msg, name);
            ck_sb_puts(&msg, ": member ");
            ck_sb_puts(&msg, mnames.v[i - 1]);
            ck_sb_puts(&msg, " has no NUL and no continuation");
            error(errors, &msg);
         }
         rows[nrows].base  = base;
         rows[nrows].size  = size;
         rows[nrows].open  = !last;
         rows[nrows].guard = mguards[i];
         nrows++;
      }
   }
   if (nrows && rows[nrows - 1].open)
   {
      ck_sb_puts(&msg, name);
      ck_sb_puts(&msg, ": the last row is unterminated");
      error(errors, &msg);
   }

   /* The pairing the menu depends on. */
   if (nrows != ids.n)
   {
      ck_sb_puts(&msg, name);
      ck_sb_puts(&msg, ": the blob holds ");
      ck_sb_putu(&msg, (unsigned long)nrows);
      ck_sb_puts(&msg, " rows but ids[] names ");
      ck_sb_putu(&msg, (unsigned long)ids.n);
      error(errors, &msg);
   }
   {
      struct ck_set names = {0};
      mh_expected_names(ids.v, ids.n, &names);
      for (i = 0; i < nrows && i < ids.n; i++)
      {
         const char *want = (const char*)*ck_set_find(&names, ids.v[i],
               strlen(ids.v[i]));
         if (strcmp(rows[i].base, want))
         {
            ck_sb_puts(&msg, name);
            ck_sb_puts(&msg, ": row ");
            ck_sb_putu(&msg, (unsigned long)i);
            ck_sb_puts(&msg, " is ");
            ck_sb_puts(&msg, rows[i].base);
            ck_sb_puts(&msg, " but ids[] pairs it with ");
            ck_sb_puts(&msg, want);
            ck_sb_puts(&msg, " (");
            ck_sb_puts(&msg, ids.v[i]);
            ck_sb_putc(&msg, ')');
            error(errors, &msg);
            break;
         }
         if (strcmp(rows[i].guard, idg.v[i]))
         {
            ck_sb_puts(&msg, name);
            ck_sb_puts(&msg, ": row ");
            ck_sb_putu(&msg, (unsigned long)i);
            ck_sb_puts(&msg, " (");
            ck_sb_puts(&msg, ids.v[i]);
            ck_sb_puts(&msg, ") is guarded ");
            put_guard(&msg, rows[i].guard);
            ck_sb_puts(&msg, " in the blob and ");
            put_guard(&msg, idg.v[i]);
            ck_sb_puts(&msg, " in ids[]");
            error(errors, &msg);
            break;
         }
      }
      free_set(&names);
   }

   /* The sizeof() check is what makes a padding compiler fail the build
    * instead of misindexing, and it does that only while it still adds
    * up to the members above it. */
   {
      unsigned long v;
      if (!size_term(sec[2].v[0].b, sec[2].v[0].e, 1, &v))
      {
         ck_sb_puts(&msg, name);
         ck_sb_puts(&msg, ": size check preamble not found");
         error(errors, &msg);
      }
      else
      {
         struct ck_set sums = {0};
         struct ck_set want = {0};
         struct guard_key *keys;
         size_t nkeys = 0, k;

         add_to(&sums, "", v);
         mh_scan_guarded(&sec[2], 1, &cl);
         for (i = 0; i < cl.n; i++)
         {
            if (!size_term(cl.v[i].b, cl.v[i].e, 0, &v))
            {
               ck_sb_puts(&msg, name);
               ck_sb_puts(&msg, ": unparsed size check term: ");
               ck_sb_add(&msg, cl.v[i].b, (size_t)(cl.v[i].e - cl.v[i].b));
               error(errors, &msg);
               continue;
            }
            add_to(&sums, cl.v[i].guard, v);
         }
         for (i = 0; i < nrows; i++)
            add_to(&want, rows[i].guard, rows[i].size);
         keys = (struct guard_key*)ck_alloc((sums.n + want.n + 1)
               * sizeof(*keys));
         for (k = 0; k < sums.cap; k++)
         {
            if (sums.keys[k])
               keys[nkeys++].guard = sums.keys[k];
         }
         for (k = 0; k < want.cap; k++)
         {
            if (want.keys[k] && !ck_set_find(&sums, want.keys[k],
                     strlen(want.keys[k])))
               keys[nkeys++].guard = want.keys[k];
         }
         for (k = 0; k < nkeys; k++)
         {
            struct ck_sb r = {0};
            mh_repr_guard(&r, keys[k].guard);
            keys[k].repr = r.p;
         }
         if (nkeys > 1)
            qsort(keys, nkeys, sizeof(*keys), cmp_key);
         for (k = 0; k < nkeys; k++)
         {
            unsigned long have = sum_of(&sums, keys[k].guard);
            unsigned long need = sum_of(&want, keys[k].guard);
            if (have != need)
            {
               ck_sb_puts(&msg, name);
               ck_sb_puts(&msg, ": the size check counts ");
               ck_sb_putu(&msg, have);
               ck_sb_puts(&msg, " bytes under ");
               ck_sb_puts(&msg, keys[k].repr);
               ck_sb_puts(&msg, ", the blob has ");
               ck_sb_putu(&msg, need);
               error(errors, &msg);
            }
            free(keys[k].repr);
         }
         free(keys);
         free_set(&sums);
         free_set(&want);
         mh_lines_free(&cl);
      }
   }

done:
   for (i = 0; i < nrows; i++)
      free(rows[i].base);
   free(rows);
   free(msizes);
   free(mguards);
   free(ilens);
   ck_strv_free(&mnames);
   ck_strv_free(&inits);
   ck_strv_free(&iguard);
   ck_strv_free(&ids);
   ck_strv_free(&idg);
   ck_sb_free(&cur);
   mh_lines_free(&ml);
   mh_lines_free(&il);
   mh_lines_free(&dl);
   for (i = 0; i < 4; i++)
      mh_lines_free(&sec[i]);
}

static int check_file(const char *path, struct ck_strv *errors)
{
   size_t len;
   char *lang = mh_lang_of(path);
   char *text = ck_read_file(path, &len);
   if (!text)
   {
      fprintf(stderr, "msg_hash_packed_check: cannot read %s\n", path);
      exit(1);
   }
   check_text(text, len, lang, path, errors);
   free(text);
   free(lang);
   return (int)errors->n;
}

/* Self-test. The fixtures are built here rather than lifted from the
 * tree, so the result says the same thing whether or not the shipped
 * headers currently pass: it measures the checker. */

static char *fixture(void)
{
   static const char *const keys[4] = {
      "MENU_ENUM_LABEL_VALUE_A", "MENU_ENUM_LABEL_VALUE_B",
      "MENU_ENUM_LABEL_VALUE_C", "MENU_ENUM_LABEL_VALUE_D"
   };
   static const char *const data[4] = { "alpha", "beta", "gamma", "delta" };
   struct ck_sb members = {0};
   struct ck_sb inits   = {0};
   struct ck_sb ids     = {0};
   struct ck_sb out     = {0};
   struct ck_set names  = {0};
   char tmp[160];
   unsigned long base = 0, guarded = 0;
   int i;

   mh_expected_names((char**)keys, 4, &names);
   for (i = 0; i < 4; i++)
   {
      const char *name = (const char*)*ck_set_find(&names, keys[i],
            strlen(keys[i]));
      int g = i == 1;
      if (i)
      {
         ck_sb_putc(&members, '\n');
         ck_sb_putc(&inits, '\n');
         ck_sb_putc(&ids, '\n');
      }
      if (g)
      {
         ck_sb_puts(&members, "#ifdef HAVE_X\n");
         ck_sb_puts(&inits, "#ifdef HAVE_X\n");
         ck_sb_puts(&ids, "#ifdef HAVE_X\n");
      }
      sprintf(tmp, "   char %s[%lu];", name,
            (unsigned long)strlen(data[i]) + 1);
      ck_sb_puts(&members, tmp);
      sprintf(tmp, "   \"%s\",", data[i]);
      ck_sb_puts(&inits, tmp);
      sprintf(tmp, "   (uint32_t)%s,", keys[i]);
      ck_sb_puts(&ids, tmp);
      if (g)
      {
         ck_sb_puts(&members, "\n#endif");
         ck_sb_puts(&inits, "\n#endif");
         ck_sb_puts(&ids, "\n#endif");
         guarded += (unsigned long)strlen(data[i]) + 1;
      }
      else
         base += (unsigned long)strlen(data[i]) + 1;
   }
   ck_sb_puts(&out, "static const struct\n{\n");
   ck_sb_puts(&out, members.p);
   ck_sb_puts(&out, "\n} msg_hash_zz_blob =\n{\n");
   ck_sb_puts(&out, inits.p);
   ck_sb_puts(&out, "\n};\n\ntypedef char msg_hash_zz_blob_check[\n");
   sprintf(tmp, "      (sizeof(msg_hash_zz_blob) == (%luu\n#ifdef HAVE_X\n"
         "       + %luu\n#endif", base, guarded);
   ck_sb_puts(&out, tmp);
   ck_sb_puts(&out, "\n      )) ? 1 : -1];\n\n");
   ck_sb_puts(&out, "static const uint32_t msg_hash_zz_ids[] =\n{\n");
   ck_sb_puts(&out, ids.p);
   ck_sb_puts(&out, "\n};\n");
   ck_sb_free(&members);
   ck_sb_free(&inits);
   ck_sb_free(&ids);
   free_set(&names);
   return out.p;
}

/* text with every from replaced by to, as str.replace() does */
static char *replace(const char *text, const char *from, const char *to)
{
   struct ck_sb out = {0};
   size_t n         = strlen(from);
   const char *p    = text;
   const char *q;
   ck_sb_puts(&out, "");
   while ((q = strstr(p, from)))
   {
      ck_sb_add(&out, p, (size_t)(q - p));
      ck_sb_puts(&out, to);
      p = q + n;
   }
   ck_sb_puts(&out, p);
   return out.p;
}

static int expect(const char *label, char *text, const char *want)
{
   struct ck_strv errors = {0};
   size_t i;
   int ok;

   check_text(text, strlen(text), "zz", label, &errors);
   if (!want)
      ok = errors.n == 0;
   else
   {
      ok = 0;
      for (i = 0; i < errors.n; i++)
         ok |= strstr(errors.v[i], want) != NULL;
   }
   printf("%-30s %s\n", label, ok ? "ok" : "FAILED");
   for (i = 0; !ok && i < errors.n; i++)
      printf("    %s\n", errors.v[i]);
   ck_strv_free(&errors);
   free(text);
   return ok ? 0 : 1;
}

static int selftest(void)
{
   static const char *const ac[2] = {
      "MENU_ENUM_LABEL_VALUE_A", "MENU_ENUM_LABEL_VALUE_C"
   };
   struct ck_set named = {0};
   struct ck_sb from   = {0};
   struct ck_sb to     = {0};
   char *good          = fixture();
   char *lost;
   const char *a, *c;
   int failures = 0;

   mh_expected_names((char**)ac, 2, &named);
   a = (const char*)*ck_set_find(&named, ac[0], strlen(ac[0]));
   c = (const char*)*ck_set_find(&named, ac[1], strlen(ac[1]));

   failures += expect("clean fixture", ck_strdup(good), NULL);

   /* A retired setting's row taken out of ids[] and left in the blob,
    * which is what renamed every row after it in 5d0f721. */
   failures += expect("ids[] row dropped",
         replace(good, "   (uint32_t)MENU_ENUM_LABEL_VALUE_C,\n", ""),
         "ids[] pairs it with");

   /* The mirror of it: the row taken out of the blob and left in ids[]. */
   ck_sb_puts(&from, "   char ");
   ck_sb_puts(&from, c);
   ck_sb_puts(&from, "[6];\n");
   lost = replace(good, from.p, "");
   failures += expect("blob row dropped",
         replace(lost, "   \"gamma\",\n", ""), "ids[] pairs it with");
   free(lost);

   /* A member one byte too wide: the walk stops on the padding NUL and
    * every row after it shifts by one. */
   from.len = 0;
   ck_sb_puts(&from, "   char ");
   ck_sb_puts(&from, a);
   ck_sb_puts(&from, "[6];\n");
   ck_sb_puts(&to, "   char ");
   ck_sb_puts(&to, a);
   ck_sb_puts(&to, "[7];\n");
   failures += expect("member wider than its row",
         replace(good, from.p, to.p), "holds 5 bytes");

   /* A guard on one side only: the pairing holds for the build that
    * defines it and breaks for the one that does not. */
   failures += expect("guard on the blob only",
         replace(good, "#ifdef HAVE_X\n   (uint32_t)MENU_ENUM_LABEL_VALUE_B,"
            "\n#endif\n", "   (uint32_t)MENU_ENUM_LABEL_VALUE_B,\n"),
         "is guarded");

   /* A contiguity constant that no longer describes the blob it
    * guards. */
   failures += expect("stale size check", replace(good, "== (18u", "== (17u"),
         "the size check counts");

   free(good);
   free_set(&named);
   ck_sb_free(&from);
   ck_sb_free(&to);
   return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
   struct ck_strv paths = {0};
   unsigned long failed = 0;
   size_t i;
   int a;

   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--selftest"))
         return selftest();
   }
   for (a = 1; a < argc; a++)
   {
      if (!strncmp(argv[a], "--", 2))
      {
         fprintf(stderr, "usage: msg_hash_packed_check [--selftest]"
               " [intl/msg_hash_xx.h ...]\n");
         return 2;
      }
      ck_strv_push(&paths, ck_strdup(argv[a]));
   }
   if (!paths.n)
   {
      const char *root = getenv("RA_TREE_ROOT");
      mh_glob_headers(root && *root ? root : ".", &paths);
   }
   for (i = 0; i < paths.n; i++)
   {
      struct ck_strv errors = {0};
      size_t k;
      check_file(paths.v[i], &errors);
      for (k = 0; k < errors.n; k++)
         printf("%s\n", errors.v[k]);
      if (errors.n)
         failed++;
      ck_strv_free(&errors);
   }
   if (failed)
   {
      printf("%lu packed header(s) failed verification\n", failed);
      return 1;
   }
   printf("%lu packed header(s) verified\n", (unsigned long)paths.n);
   return 0;
}

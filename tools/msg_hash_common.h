/* What tools/msg_hash_packed_check.c and tools/msg_hash_format_check.c
 * both read in a packed intl/msg_hash_<lang>.h: the four regions
 * intl/json2h.py emits, their rows and the guards around them.
 *
 * A guard is the stack of #if lines around a row, each with whether
 * its #else has been passed, kept as one string: per level '0' or '1'
 * and the stripped #if line, then a newline. */
#ifndef TOOLS_MSG_HASH_COMMON_H
#define TOOLS_MSG_HASH_COMMON_H

#include "check_common.h"

/* A line of text, [b, e), and the guard around it. */
struct mh_line
{
   const char *b;
   const char *e;
   char *guard;
};

struct mh_lines
{
   struct mh_line *v;
   size_t n;
};

/* The length of the white space character at p, as str.strip() counts
 * white space in UTF-8 text, or 0. */
CK_FN size_t mh_space_at(const char *p, const char *e)
{
   const unsigned char *u = (const unsigned char*)p;
   size_t n               = (size_t)(e - p);
   if (!n)
      return 0;
   if (CK_IS_SPACE(u[0]) || (u[0] >= 0x1c && u[0] <= 0x1f))
      return 1;
   if (n >= 2 && u[0] == 0xc2 && (u[1] == 0x85 || u[1] == 0xa0))
      return 2;
   if (n >= 3 && u[0] == 0xe1 && u[1] == 0x9a && u[2] == 0x80)
      return 3;
   if (n >= 3 && u[0] == 0xe2 && u[1] == 0x80
         && (u[2] <= 0x8a || u[2] == 0xa8 || u[2] == 0xa9 || u[2] == 0xaf))
      return 3;
   if (n >= 3 && u[0] == 0xe2 && u[1] == 0x81 && u[2] == 0x9f)
      return 3;
   if (n >= 3 && u[0] == 0xe3 && u[1] == 0x80 && u[2] == 0x80)
      return 3;
   return 0;
}

/* The length of the white space character ending at e, or 0. */
CK_FN size_t mh_space_before(const char *b, const char *e)
{
   size_t k;
   for (k = 1; k <= 3 && (size_t)(e - b) >= k; k++)
   {
      if (mh_space_at(e - k, e) == k)
         return k;
   }
   return 0;
}

CK_FN void mh_strip(const char **b, const char **e)
{
   size_t k;
   while (*b < *e && (k = mh_space_at(*b, *e)))
      *b += k;
   while (*e > *b && (k = mh_space_before(*b, *e)))
      *e -= k;
}

/* Whether [b, e) with white space stripped from its end ends with c. */
CK_FN int mh_rstrip_ends_with(const char *b, const char *e, char c)
{
   size_t k;
   while (e > b && (k = mh_space_before(b, e)))
      e -= k;
   return e > b && e[-1] == c;
}

CK_FN int mh_starts(const char *b, const char *e, const char *s)
{
   size_t n = strlen(s);
   return (size_t)(e - b) >= n && !memcmp(b, s, n);
}

/* The lines of [b, e) split at '\n', as str.split('\n') does. */
CK_FN void mh_split(const char *b, const char *e, struct mh_lines *out)
{
   const char *p = b;
   out->v = NULL;
   out->n = 0;
   for (;;)
   {
      const char *nl = (const char*)memchr(p, '\n', (size_t)(e - p));
      out->v = (struct mh_line*)ck_realloc(out->v,
            (out->n + 1) * sizeof(*out->v));
      out->v[out->n].b     = p;
      out->v[out->n].e     = nl ? nl : e;
      out->v[out->n].guard = NULL;
      out->n++;
      if (!nl)
         break;
      p = nl + 1;
   }
}

CK_FN void mh_lines_free(struct mh_lines *l)
{
   size_t i;
   for (i = 0; i < l->n; i++)
      free(l->v[i].guard);
   free(l->v);
   l->v = NULL;
   l->n = 0;
}

/* Every line that is not blank and not an #if, #else or #endif, with
 * the stack of #if lines around it. */
CK_FN void mh_scan_guarded(const struct mh_lines *in, size_t first,
      struct mh_lines *out)
{
   struct ck_strv stack = {0};
   size_t i;

   out->v = NULL;
   out->n = 0;
   for (i = first; i < in->n; i++)
   {
      const char *b = in->v[i].b;
      const char *e = in->v[i].e;
      mh_strip(&b, &e);
      if (mh_starts(b, e, "#if"))
      {
         struct ck_sb sb = {0};
         ck_sb_putc(&sb, '0');
         ck_sb_add(&sb, b, (size_t)(e - b));
         ck_strv_push(&stack, sb.p);
      }
      else if (mh_starts(b, e, "#else"))
      {
         if (stack.n)
            stack.v[stack.n - 1][0] = '1';
      }
      else if (mh_starts(b, e, "#endif"))
      {
         if (stack.n)
            free(stack.v[--stack.n]);
      }
      else if (b < e)
      {
         struct ck_sb g = {0};
         size_t k;
         ck_sb_puts(&g, "");
         for (k = 0; k < stack.n; k++)
         {
            ck_sb_puts(&g, stack.v[k]);
            ck_sb_putc(&g, '\n');
         }
         out->v = (struct mh_line*)ck_realloc(out->v,
               (out->n + 1) * sizeof(*out->v));
         out->v[out->n].b     = in->v[i].b;
         out->v[out->n].e     = in->v[i].e;
         out->v[out->n].guard = g.p;
         out->n++;
      }
   }
   ck_strv_free(&stack);
}

/* A guard as Python's repr() writes the tuple json2h.py keeps. */
CK_FN void mh_repr_guard(struct ck_sb *sb, const char *guard)
{
   const char *p   = guard;
   unsigned levels = 0;
   ck_sb_putc(sb, '(');
   while (*p)
   {
      const char *nl = strchr(p, '\n');
      if (levels++)
         ck_sb_puts(sb, ", ");
      ck_sb_putc(sb, '(');
      ck_repr(sb, p + 1, nl);
      ck_sb_puts(sb, *p == '1' ? ", True)" : ", False)");
      p = nl + 1;
   }
   if (levels == 1)
      ck_sb_putc(sb, ',');
   ck_sb_putc(sb, ')');
}

CK_FN int mh_is_hex(int c)
{
   return CK_IS_DIGIT(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* The text between the quotes of a C string literal, decoded. */
CK_FN void mh_decode_literal(const char *b, const char *e, struct ck_sb *out)
{
   const char *p = b;
   while (p < e)
   {
      const char *j;
      unsigned v = 0;
      if (*p != '\\')
      {
         ck_sb_putc(out, *p++);
         continue;
      }
      p++;
      if (p >= e)
         break;
      switch (*p)
      {
         case 'n':
            ck_sb_putc(out, '\n');
            p++;
            break;
         case 't':
            ck_sb_putc(out, '\t');
            p++;
            break;
         case 'r':
            ck_sb_putc(out, '\r');
            p++;
            break;
         case 'x':
            for (j = p + 1; j < e && mh_is_hex(*j); j++)
               v = (v << 4) | (unsigned)(CK_IS_DIGIT(*j) ? *j - '0'
                     : (*j | 0x20) - 'a' + 10);
            ck_sb_putc(out, (char)(v & 0xff));
            p = j;
            break;
         default:
            if (*p >= '0' && *p <= '7')
            {
               for (j = p; j < e && j < p + 3 && *j >= '0' && *j <= '7'; j++)
                  v = (v << 3) | (unsigned)(*j - '0');
               ck_sb_putc(out, (char)(v & 0xff));
               p = j;
            }
            else
               ck_sb_putc(out, *p++);
            break;
      }
   }
}

/* Every string literal on a line, decoded and run together:
 * "((?:[^"\\]|\\.)*)"  - an opening quote whose literal does not close
 * starts nothing, and the search goes on from the byte after it. */
CK_FN void mh_line_literals(const char *b, const char *e, struct ck_sb *out)
{
   const char *p = b;
   while ((p = (const char*)memchr(p, '"', (size_t)(e - p))))
   {
      const char *q = p + 1;
      while (q < e && *q != '"')
      {
         if (*q == '\\')
         {
            if (q + 1 >= e)
               break;
            q += 2;
         }
         else
            q++;
      }
      if (q < e && *q == '"')
      {
         mh_decode_literal(p + 1, q, out);
         p = q + 1;
      }
      else
         p++;
   }
}

/* \s* with str's white space, from p */
CK_FN const char *mh_skip_space(const char *p, const char *e)
{
   size_t k;
   while (p < e && (k = mh_space_at(p, e)))
      p += k;
   return p;
}

/* \s*char (\w+)\[(\d+)\];  Returns 0 when the line is not one. */
CK_FN int mh_member(const char *b, const char *e, char **name,
      unsigned long *size)
{
   const char *p = mh_skip_space(b, e);
   const char *w, *d;
   if (!mh_starts(p, e, "char "))
      return 0;
   for (w = p += 5; p < e && CK_IS_WORD(*p); p++);
   if (p == w || p >= e || *p != '[')
      return 0;
   for (d = ++p; p < e && CK_IS_DIGIT(*p); p++);
   if (p == d || e - p < 2 || p[0] != ']' || p[1] != ';')
      return 0;
   *name = ck_strndup(w, (size_t)(d - 1 - w));
   *size = strtoul(d, NULL, 10);
   return 1;
}

/* \s*\(msg_hash_id_t\)(\w+),  Returns the name or NULL. */
CK_FN char *mh_ident(const char *b, const char *e)
{
   const char *p = mh_skip_space(b, e);
   const char *w;
   if (!mh_starts(p, e, "(msg_hash_id_t)"))
      return NULL;
   for (w = p += 15; p < e && CK_IS_WORD(*p); p++);
   if (p == w || p >= e || *p != ',')
      return NULL;
   return ck_strndup(w, (size_t)(p - w));
}

CK_FN const char *mh_find(const char *hay, const char *end, const char *s,
      size_t from)
{
   size_t n = strlen(s);
   const char *p;
   for (p = hay + from; (size_t)(end - p) >= n; p++)
   {
      if (*p == *s && !memcmp(p, s, n))
         return p;
   }
   return NULL;
}

/* The four generated regions of a packed header as lines: members,
 * initialisers, the size check and ids[]. Returns 0 when one is
 * missing. */
CK_FN int mh_split_sections(const char *text, size_t len, const char *lang,
      struct mh_lines *sec)
{
   static const char *const starts[4] = {
      "static const struct\n{\n", "_blob =\n{\n", "_blob_check[\n",
      "_ids[] =\n{\n"
   };
   const char *end = text + len;
   struct ck_sb first_end = {0};
   size_t at = 0;
   int k;

   ck_sb_puts(&first_end, "\n} msg_hash_");
   ck_sb_puts(&first_end, lang);
   ck_sb_puts(&first_end, "_blob =");
   for (k = 0; k < 4; k++)
   {
      const char *ends = k == 0 ? first_end.p
         : k == 2 ? "\n      )) ? 1 : -1];" : "\n};";
      const char *i = mh_find(text, end, starts[k], at);
      const char *j;
      if (i)
         i += strlen(starts[k]);
      if (!i || !(j = mh_find(text, end, ends, (size_t)(i - text))))
      {
         while (k--)
            mh_lines_free(&sec[k]);
         ck_sb_free(&first_end);
         return 0;
      }
      mh_split(i, j, &sec[k]);
      at = (size_t)(j - text);
   }
   ck_sb_free(&first_end);
   return 1;
}

CK_FN unsigned long mh_djb2(const char *s)
{
   unsigned long h = 5381;
   for (; *s; s++)
      h = (h * 33 + (unsigned char)*s) & 0xffffffffUL;
   return h;
}

struct mh_named
{
   const char *key;
   unsigned long hash;
};

CK_FN int mh_cmp_named(const void *a, const void *b)
{
   const struct mh_named *x = (const struct mh_named*)a;
   const struct mh_named *y = (const struct mh_named*)b;
   if (x->hash != y->hash)
      return x->hash < y->hash ? -1 : 1;
   return strcmp(x->key, y->key);
}

/* json2h.py's member_base_names(): key -> s_<hash>, or s_<hash>_c<n>
 * among keys that share a hash, in key order. */
CK_FN void mh_expected_names(char **keys, size_t n, struct ck_set *names)
{
   struct mh_named *v = (struct mh_named*)ck_alloc((n + 1) * sizeof(*v));
   struct ck_set seen = {0};
   size_t i, m = 0;

   for (i = 0; i < n; i++)
   {
      void **slot = ck_set_add(&seen, keys[i], strlen(keys[i]));
      if (*slot)
         continue;
      *slot       = &seen;
      v[m].key    = keys[i];
      v[m].hash   = mh_djb2(keys[i]);
      m++;
   }
   if (m > 1)
      qsort(v, m, sizeof(*v), mh_cmp_named);
   for (i = 0; i < m; )
   {
      size_t j, k;
      for (j = i; j < m && v[j].hash == v[i].hash; j++);
      for (k = i; k < j; k++)
      {
         char tmp[48];
         void **slot = ck_set_add(names, v[k].key, strlen(v[k].key));
         if (j - i == 1)
            sprintf(tmp, "s_%08lx", v[k].hash);
         else
            sprintf(tmp, "s_%08lx_c%lu", v[k].hash, (unsigned long)(k - i));
         free(*slot);
         *slot = ck_strdup(tmp);
      }
      i = j;
   }
   for (i = 0; i < seen.cap; i++)
      free(seen.keys[i]);
   free(seen.keys);
   free(seen.vals);
   free(v);
}

/* The language of intl/msg_hash_<lang>.h, as basename[9:-2] slices it. */
CK_FN char *mh_lang_of(const char *path)
{
   const char *s = path + strlen(path);
   size_t n, from, to;
   while (s > path && s[-1] != '/'
#ifdef _WIN32
         && s[-1] != '\\'
#endif
         )
      s--;
   n    = strlen(s);
   from = n < 9 ? n : 9;
   to   = n < 2 ? 0 : n - 2;
   return to > from ? ck_strndup(s + from, to - from) : ck_strdup("");
}

CK_FN int mh_has_ids(const char *text, size_t len, const char *lang,
      const char *type)
{
   struct ck_sb marker = {0};
   int found;
   ck_sb_puts(&marker, "static const ");
   ck_sb_puts(&marker, type);
   ck_sb_puts(&marker, " msg_hash_");
   ck_sb_puts(&marker, lang);
   ck_sb_puts(&marker, "_ids[] =");
   found = mh_find(text, text + len, marker.p, 0) != NULL;
   ck_sb_free(&marker);
   return found;
}

CK_FN int mh_is_packed(const char *text, size_t len, const char *lang)
{
   return mh_has_ids(text, len, lang, "msg_hash_id_t");
}

/* An ids[] array of any other type: a packed header that was not
 * regenerated when the row id type changed. */
CK_FN int mh_is_stale_packed(const char *text, size_t len, const char *lang)
{
   struct ck_sb marker = {0};
   const char *p, *e = text + len;
   int found = 0;
   ck_sb_puts(&marker, " msg_hash_");
   ck_sb_puts(&marker, lang);
   ck_sb_puts(&marker, "_ids[] =");
   p = mh_find(text, e, marker.p, 0);
   if (p && !mh_is_packed(text, len, lang))
      found = 1;
   ck_sb_free(&marker);
   return found;
}

/* The intl/msg_hash_*.h files under root, sorted, as glob finds them. */
CK_FN void mh_glob_headers(const char *root, struct ck_strv *paths)
{
   struct ck_strv names = {0};
   struct ck_sb dir     = {0};
   size_t i;

   ck_sb_puts(&dir, root);
   ck_sb_puts(&dir, "/intl");
   ck_list_dir(dir.p, &names);
   for (i = 0; i < names.n; i++)
   {
      if (ck_starts_with(names.v[i], "msg_hash_")
            && ck_ends_with(names.v[i], ".h") && strlen(names.v[i]) >= 11)
      {
         struct ck_sb p = {0};
         ck_sb_puts(&p, dir.p);
         ck_sb_putc(&p, '/');
         ck_sb_puts(&p, names.v[i]);
         ck_strv_push(paths, p.p);
      }
   }
   ck_strv_sort(paths);
   ck_strv_free(&names);
   ck_sb_free(&dir);
}

#endif

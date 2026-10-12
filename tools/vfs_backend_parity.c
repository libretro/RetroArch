/* Check that every VFS backend defines the same retro_vfs_*_impl set.
 *
 * vfs_implementation_uwp.cpp does not extend vfs_implementation.c, it
 * replaces it: the UWP project compiles the .cpp and not the .c. So a
 * function added to the C backend and called from file_stream.c - which
 * both builds share - compiles fine everywhere and fails to link on UWP
 * only, at the end of a long build, on a runner most contributors do not
 * have. That is how retro_vfs_file_get_mapped_ptr_impl shipped broken.
 *
 * This compares the two backends by symbol and fails on any function the
 * C backend defines and the UWP backend does not. It is a source-level
 * check, so it costs nothing and runs on Linux.
 *
 * A backend that genuinely cannot support a call still has to say so -
 * returning a documented "not available" value, the way the UWP mapping
 * accessor returns NULL - because a missing definition is a link error,
 * not a fallback.
 *
 * Usage, from the repository root: tools/check.sh vfs_backend_parity */

#include "check_common.h"

#define C_BACKEND   "libretro-common/vfs/vfs_implementation.c"
#define UWP_BACKEND "libretro-common/vfs/vfs_implementation_uwp.cpp"

#define IN_CLASS(c) (CK_IS_WORD(c) || (c) == '*' || CK_IS_SPACE(c))

/* Definitions only: a return type (possibly with const/ * /ws) then the
 * name then '(' at the start of a line. Calls are indented, so anchoring
 * to column 0 keeps them out.
 * ^(?:const\s+|static\s+|struct\s+)*[\w\*\s]*?\b(retro_vfs_\w+_impl)\s*\(
 * (re.M): from a line start, over [\w*\s] only, the first word that is
 * such a name with its parenthesis. */
static void defined_in(const char *path, struct ck_set *names)
{
   static const char pre[] = "retro_vfs_";
   size_t len, i;
   size_t *end;
   char *t = ck_read_file(path, &len);

   if (!t)
      return;
   /* end[i]: past the '(' when a name of the kind starts at i, else 0 */
   end = (size_t*)ck_alloc((len + 1) * sizeof(size_t));
   for (i = 0; i <= len; i++)
      end[i] = 0;
   for (i = 0; i < len; i++)
   {
      size_t r, q;
      if (!CK_IS_WORD(t[i]) || (i && CK_IS_WORD(t[i - 1])))
         continue;
      for (r = i; r < len && CK_IS_WORD(t[r]); r++);
      if (r - i < sizeof(pre) - 1 + 1 + 5 || memcmp(t + i, pre, sizeof(pre) - 1)
            || memcmp(t + r - 5, "_impl", 5))
         continue;
      for (q = r; q < len && CK_IS_SPACE(t[q]); q++);
      if (q < len && t[q] == '(')
         end[i] = q + 1;
   }
   /* from each line start, the first candidate before a byte outside
    * the class */
   {
      long *cand = (long*)ck_alloc((len + 1) * sizeof(long));
      long *out  = (long*)ck_alloc((len + 1) * sizeof(long));
      size_t s   = 0;
      long k;
      cand[len] = -1;
      out[len]  = (long)len;
      for (k = (long)len - 1; k >= 0; k--)
      {
         cand[k] = end[k] ? k : cand[k + 1];
         out[k]  = IN_CLASS(t[k]) ? out[k + 1] : k;
      }
      while (s < len)
      {
         long c = cand[s];
         if (c >= 0 && c <= out[s])
         {
            const char *w = t + c;
            size_t r;
            void **slot;
            for (r = (size_t)c; r < len && CK_IS_WORD(t[r]); r++);
            slot  = ck_set_add(names, w, r - (size_t)c);
            *slot = names;
            s     = end[c];
            /* the next line start at or after the match end */
            while (s < len && s > 0 && t[s - 1] != '\n')
               s++;
            continue;
         }
         {
            const char *nl = (const char*)memchr(t + s, '\n', len - s);
            if (!nl)
               break;
            s = (size_t)(nl - t) + 1;
         }
      }
      free(cand);
      free(out);
   }
   free(end);
   free(t);
}

int main(int argc, char **argv)
{
   struct ck_set c       = {0};
   struct ck_set uwp     = {0};
   struct ck_strv missing = {0};
   size_t i;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: vfs_backend_parity\n");
      return 2;
   }
   defined_in(C_BACKEND, &c);
   defined_in(UWP_BACKEND, &uwp);
   if (!c.n || !uwp.n)
   {
      fprintf(stderr, "error: parsed no symbols from a backend; the regex"
            " or the file layout has drifted\n");
      return 2;
   }
   for (i = 0; i < c.cap; i++)
   {
      if (c.keys[i] && !ck_set_find(&uwp, c.keys[i], strlen(c.keys[i])))
         ck_strv_push(&missing, ck_strdup(c.keys[i]));
   }
   if (missing.n)
   {
      ck_strv_sort(&missing);
      fprintf(stderr, "error: defined in %s but missing from %s:\n",
            C_BACKEND, UWP_BACKEND);
      for (i = 0; i < missing.n; i++)
         fprintf(stderr, "   %s\n", missing.v[i]);
      fprintf(stderr, "\nAdd each to the UWP backend.  If it cannot be"
            " supported there, define it anyway and return the documented"
            " \"unavailable\" value - a missing definition is a link"
            " error, not a fallback.\n");
      return 1;
   }
   printf("vfs backend parity: %lu symbols, both backends agree\n",
         (unsigned long)c.n);
   return 0;
}

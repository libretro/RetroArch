/* Shared by the tree checkers in tools/ that are written in C
 * (run through tools/check.sh). C89, no dependency but libc.
 *
 * Text is read as bytes: a word character is [A-Za-z0-9_], space is
 * the C locale's, and a byte above 0x7f is neither. */
#ifndef TOOLS_CHECK_COMMON_H
#define TOOLS_CHECK_COMMON_H

#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700 /* lstat, S_ISLNK under -std=c89 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#if defined(__GNUC__)
#define CK_FN static __attribute__((unused))
#else
#define CK_FN static
#endif

#define CK_IS_WORD(c)  (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') \
      || ((c) >= '0' && (c) <= '9') || (c) == '_')
#define CK_IS_IDENT0(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') \
      || (c) == '_')
#define CK_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
/* \s of a byte pattern: space, \t \n \v \f \r */
#define CK_IS_SPACE(c) ((c) == ' ' || ((c) >= '\t' && (c) <= '\r'))

CK_FN void ck_die(const char *msg)
{
   fprintf(stderr, "%s\n", msg);
   exit(2);
}

CK_FN void *ck_alloc(size_t n)
{
   void *p = malloc(n ? n : 1);
   if (!p)
      ck_die("out of memory");
   return p;
}

CK_FN void *ck_realloc(void *p, size_t n)
{
   p = realloc(p, n ? n : 1);
   if (!p)
      ck_die("out of memory");
   return p;
}

CK_FN char *ck_strndup(const char *s, size_t n)
{
   char *p = (char*)ck_alloc(n + 1);
   memcpy(p, s, n);
   p[n] = '\0';
   return p;
}

CK_FN char *ck_strdup(const char *s)
{
   return ck_strndup(s, strlen(s));
}

/* A growing string. p is always NUL-terminated once written to. */
struct ck_sb
{
   char *p;
   size_t len;
   size_t cap;
};

CK_FN void ck_sb_reserve(struct ck_sb *sb, size_t extra)
{
   if (sb->len + extra + 1 > sb->cap)
   {
      size_t cap = sb->cap ? sb->cap * 2 : 256;
      while (cap < sb->len + extra + 1)
         cap *= 2;
      sb->p   = (char*)ck_realloc(sb->p, cap);
      sb->cap = cap;
   }
}

CK_FN void ck_sb_add(struct ck_sb *sb, const char *s, size_t n)
{
   ck_sb_reserve(sb, n);
   memcpy(sb->p + sb->len, s, n);
   sb->len       += n;
   sb->p[sb->len] = '\0';
}

CK_FN void ck_sb_puts(struct ck_sb *sb, const char *s)
{
   ck_sb_add(sb, s, strlen(s));
}

CK_FN void ck_sb_putc(struct ck_sb *sb, char c)
{
   ck_sb_add(sb, &c, 1);
}

CK_FN void ck_sb_putu(struct ck_sb *sb, unsigned long v)
{
   char tmp[32];
   sprintf(tmp, "%lu", v);
   ck_sb_puts(sb, tmp);
}

CK_FN void ck_sb_free(struct ck_sb *sb)
{
   free(sb->p);
   sb->p   = NULL;
   sb->len = sb->cap = 0;
}

/* A growing array of strings, each owned. */
struct ck_strv
{
   char **v;
   size_t n;
   size_t cap;
};

CK_FN void ck_strv_push(struct ck_strv *sv, char *s)
{
   if (sv->n == sv->cap)
   {
      sv->cap = sv->cap ? sv->cap * 2 : 16;
      sv->v   = (char**)ck_realloc(sv->v, sv->cap * sizeof(char*));
   }
   sv->v[sv->n++] = s;
}

CK_FN void ck_strv_free(struct ck_strv *sv)
{
   size_t i;
   for (i = 0; i < sv->n; i++)
      free(sv->v[i]);
   free(sv->v);
   sv->v = NULL;
   sv->n = sv->cap = 0;
}

CK_FN int ck_cmp_str(const void *a, const void *b)
{
   return strcmp(*(char * const *)a, *(char * const *)b);
}

CK_FN void ck_strv_sort(struct ck_strv *sv)
{
   if (sv->n > 1)
      qsort(sv->v, sv->n, sizeof(char*), ck_cmp_str);
}

CK_FN int ck_ends_with(const char *s, const char *suffix)
{
   size_t n = strlen(s);
   size_t m = strlen(suffix);
   return n >= m && !memcmp(s + n - m, suffix, m);
}

CK_FN int ck_starts_with(const char *s, const char *prefix)
{
   return !strncmp(s, prefix, strlen(prefix));
}

/* The whole file with \r\n and a lone \r read as \n, or NULL. */
CK_FN char *ck_read_file(const char *path, size_t *len)
{
   size_t n = 0;
   size_t cap = 0;
   size_t i, o;
   char *buf  = NULL;
   FILE *f    = fopen(path, "rb");
   if (!f)
      return NULL;
   for (;;)
   {
      size_t got;
      if (cap - n < 65536)
      {
         cap = cap ? cap * 2 : 131072;
         buf = (char*)ck_realloc(buf, cap + 1);
      }
      got = fread(buf + n, 1, cap - n, f);
      n  += got;
      if (!got)
         break;
   }
   fclose(f);
   for (i = 0, o = 0; i < n; i++)
   {
      if (buf[i] == '\r')
      {
         buf[o++] = '\n';
         if (i + 1 < n && buf[i + 1] == '\n')
            i++;
      }
      else
         buf[o++] = buf[i];
   }
   buf[o] = '\0';
   if (len)
      *len = o;
   return buf;
}

CK_FN int ck_write_file(const char *path, const char *text)
{
   FILE *f = fopen(path, "wb");
   if (!f)
      return 0;
   fwrite(text, 1, strlen(text), f);
   return fclose(f) == 0;
}

CK_FN int ck_is_dir(const char *path)
{
   struct stat st;
   return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

CK_FN int ck_is_file(const char *path)
{
   struct stat st;
   return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

CK_FN int ck_is_link(const char *path)
{
#ifdef _WIN32
   (void)path;
   return 0;
#else
   struct stat st;
   return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
#endif
}

/* Names in a directory, in the order the system lists them. */
CK_FN int ck_list_dir(const char *path, struct ck_strv *names)
{
#ifdef _WIN32
   WIN32_FIND_DATAA fd;
   HANDLE h;
   struct ck_sb pat = {0};
   ck_sb_puts(&pat, path);
   ck_sb_puts(&pat, "\\*");
   h = FindFirstFileA(pat.p, &fd);
   ck_sb_free(&pat);
   if (h == INVALID_HANDLE_VALUE)
      return 0;
   do
   {
      if (strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
         ck_strv_push(names, ck_strdup(fd.cFileName));
   } while (FindNextFileA(h, &fd));
   FindClose(h);
   return 1;
#else
   struct dirent *e;
   DIR *d = opendir(path);
   if (!d)
      return 0;
   while ((e = readdir(d)))
   {
      if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
         ck_strv_push(names, ck_strdup(e->d_name));
   }
   closedir(d);
   return 1;
#endif
}

/* Walks a tree top-down: a directory's files in listing order, then
 * its subdirectories in listing order. rel is the path below the
 * root with '/' separators ("" for the root). skip_dir() prunes a
 * subdirectory by name; a symbolic link to a directory is not
 * entered. Either callback may be NULL. */
typedef int  (*ck_skip_dir_fn)(const char *name, const char *rel, void *ud);
typedef void (*ck_file_fn)(const char *path, const char *rel, void *ud);

CK_FN void ck_walk_in(struct ck_sb *path, struct ck_sb *rel,
      ck_skip_dir_fn skip_dir, ck_file_fn on_file, void *ud)
{
   size_t i;
   struct ck_strv names = {0};
   struct ck_strv dirs  = {0};
   size_t plen          = path->len;
   size_t rlen          = rel->len;

   if (!ck_list_dir(path->p, &names))
      return;
   for (i = 0; i < names.n; i++)
   {
      ck_sb_putc(path, '/');
      ck_sb_puts(path, names.v[i]);
      if (rlen)
         ck_sb_putc(rel, '/');
      ck_sb_puts(rel, names.v[i]);
      if (ck_is_dir(path->p))
      {
         if (!(skip_dir && skip_dir(names.v[i], rel->p, ud))
               && !ck_is_link(path->p))
            ck_strv_push(&dirs, ck_strdup(names.v[i]));
      }
      else if (on_file)
         on_file(path->p, rel->p, ud);
      path->len = plen;
      path->p[plen] = '\0';
      rel->len = rlen;
      rel->p[rlen] = '\0';
   }
   for (i = 0; i < dirs.n; i++)
   {
      ck_sb_putc(path, '/');
      ck_sb_puts(path, dirs.v[i]);
      if (rlen)
         ck_sb_putc(rel, '/');
      ck_sb_puts(rel, dirs.v[i]);
      ck_walk_in(path, rel, skip_dir, on_file, ud);
      path->len = plen;
      path->p[plen] = '\0';
      rel->len = rlen;
      rel->p[rlen] = '\0';
   }
   ck_strv_free(&names);
   ck_strv_free(&dirs);
}

CK_FN void ck_walk(const char *root, ck_skip_dir_fn skip_dir,
      ck_file_fn on_file, void *ud)
{
   struct ck_sb path = {0};
   struct ck_sb rel  = {0};
   ck_sb_puts(&path, root);
   /* "dir/" and "dir" name the same root */
   while (path.len > 1 && (path.p[path.len - 1] == '/'
            || path.p[path.len - 1] == '\\'))
      path.p[--path.len] = '\0';
   ck_sb_puts(&rel, "");
   ck_walk_in(&path, &rel, skip_dir, on_file, ud);
   ck_sb_free(&path);
   ck_sb_free(&rel);
}

CK_FN int ck_mkdir(const char *path)
{
#ifdef _WIN32
   return _mkdir(path) == 0;
#else
   return mkdir(path, 0700) == 0;
#endif
}

/* Makes every missing directory above a file path. */
CK_FN void ck_mkdirs_for(const char *file)
{
   char *p = ck_strdup(file);
   char *s;
   for (s = p + 1; *s; s++)
   {
      if (*s == '/')
      {
         *s = '\0';
         if (!ck_is_dir(p))
            ck_mkdir(p);
         *s = '/';
      }
   }
   free(p);
}

/* A new empty directory for a self-test's fixtures; the caller frees
 * the name after ck_rmtree(). */
CK_FN char *ck_tmpdir(void)
{
   static unsigned seq;
   struct ck_sb sb = {0};
   const char *base = getenv("TMPDIR");
   unsigned tries;
#ifdef _WIN32
   if (!base)
      base = getenv("TEMP");
   if (!base)
      base = ".";
#else
   if (!base)
      base = "/tmp";
#endif
   for (tries = 0; tries < 1000; tries++)
   {
      char tail[64];
#ifdef _WIN32
      sprintf(tail, "/ck%lu_%u", (unsigned long)GetCurrentProcessId(), seq++);
#else
      sprintf(tail, "/ck%lu_%u", (unsigned long)getpid(), seq++);
#endif
      sb.len = 0;
      ck_sb_puts(&sb, base);
      ck_sb_puts(&sb, tail);
      if (ck_mkdir(sb.p))
         return sb.p;
   }
   ck_die("cannot make a temporary directory");
   return NULL;
}

CK_FN void ck_rmtree(const char *path)
{
   size_t i;
   struct ck_strv names = {0};
   ck_list_dir(path, &names);
   for (i = 0; i < names.n; i++)
   {
      struct ck_sb sb = {0};
      ck_sb_puts(&sb, path);
      ck_sb_putc(&sb, '/');
      ck_sb_puts(&sb, names.v[i]);
      if (ck_is_dir(sb.p) && !ck_is_link(sb.p))
         ck_rmtree(sb.p);
      else
         remove(sb.p);
      ck_sb_free(&sb);
   }
   ck_strv_free(&names);
#ifdef _WIN32
   _rmdir(path);
#else
   rmdir(path);
#endif
}

/* A set of strings (open addressing, FNV-1a), each with a value. */
struct ck_set
{
   char **keys;
   void **vals;
   size_t cap;
   size_t n;
};

CK_FN unsigned long ck_hash(const char *s, size_t n)
{
   unsigned long h = 2166136261UL;
   size_t i;
   for (i = 0; i < n; i++)
      h = ((h ^ (unsigned char)s[i]) * 16777619UL) & 0xffffffffUL;
   return h;
}

CK_FN size_t ck_set_slot(const struct ck_set *set, const char *s, size_t n)
{
   size_t i = (size_t)ck_hash(s, n) & (set->cap - 1);
   while (set->keys[i]
         && (strncmp(set->keys[i], s, n) || set->keys[i][n]))
      i = (i + 1) & (set->cap - 1);
   return i;
}

/* The slot of a key of n bytes, or NULL. */
CK_FN void **ck_set_find(const struct ck_set *set, const char *s, size_t n)
{
   size_t i;
   if (!set->cap)
      return NULL;
   i = ck_set_slot(set, s, n);
   return set->keys[i] ? &set->vals[i] : NULL;
}

/* The slot of a key, added when it is new. */
CK_FN void **ck_set_add(struct ck_set *set, const char *s, size_t n)
{
   size_t i;
   if ((set->n + 1) * 2 > set->cap)
   {
      struct ck_set big;
      big.cap  = set->cap ? set->cap * 2 : 64;
      big.n    = set->n;
      big.keys = (char**)ck_alloc(big.cap * sizeof(char*));
      big.vals = (void**)ck_alloc(big.cap * sizeof(void*));
      memset(big.keys, 0, big.cap * sizeof(char*));
      memset(big.vals, 0, big.cap * sizeof(void*));
      for (i = 0; i < set->cap; i++)
      {
         if (set->keys[i])
         {
            size_t j    = ck_set_slot(&big, set->keys[i], strlen(set->keys[i]));
            big.keys[j] = set->keys[i];
            big.vals[j] = set->vals[i];
         }
      }
      free(set->keys);
      free(set->vals);
      *set = big;
   }
   i = ck_set_slot(set, s, n);
   if (!set->keys[i])
   {
      set->keys[i] = ck_strndup(s, n);
      set->n++;
   }
   return &set->vals[i];
}

CK_FN void ck_set_free(struct ck_set *set)
{
   size_t i;
   for (i = 0; i < set->cap; i++)
      free(set->keys[i]);
   free(set->keys);
   free(set->vals);
   memset(set, 0, sizeof(*set));
}

/* A string as Python's repr() writes it: UTF-8 text, with the
 * characters str.isprintable() refuses escaped (the common ones). */
CK_FN void ck_repr(struct ck_sb *sb, const char *b, const char *e)
{
   char quote = '\'';
   const unsigned char *p;
   if (memchr(b, '\'', (size_t)(e - b)) && !memchr(b, '"', (size_t)(e - b)))
      quote = '"';
   ck_sb_putc(sb, quote);
   for (p = (const unsigned char*)b; p < (const unsigned char*)e; )
   {
      char tmp[16];
      unsigned long cp;
      size_t n = 1;
      if (*p < 0x80)
      {
         if (*p == (unsigned char)quote || *p == '\\')
         {
            ck_sb_putc(sb, '\\');
            ck_sb_putc(sb, (char)*p);
         }
         else if (*p == '\t')
            ck_sb_puts(sb, "\\t");
         else if (*p == '\n')
            ck_sb_puts(sb, "\\n");
         else if (*p == '\r')
            ck_sb_puts(sb, "\\r");
         else if (*p < 0x20 || *p == 0x7f)
         {
            sprintf(tmp, "\\x%02x", *p);
            ck_sb_puts(sb, tmp);
         }
         else
            ck_sb_putc(sb, (char)*p);
         p++;
         continue;
      }
      if (*p >= 0xf0)
      {
         n  = 4;
         cp = *p & 0x07;
      }
      else if (*p >= 0xe0)
      {
         n  = 3;
         cp = *p & 0x0f;
      }
      else
      {
         n  = 2;
         cp = *p & 0x1f;
      }
      if ((size_t)((const unsigned char*)e - p) < n)
         n = (size_t)((const unsigned char*)e - p);
      {
         size_t k;
         for (k = 1; k < n; k++)
            cp = (cp << 6) | (p[k] & 0x3f);
      }
      /* the characters str.isprintable() refuses, the common ones */
      if (     (cp >= 0x80 && cp <= 0xa0) || cp == 0xad || cp == 0x1680
            || (cp >= 0x2000 && cp <= 0x200f) || (cp >= 0x2028 && cp <= 0x202f)
            || (cp >= 0x205f && cp <= 0x206f) || cp == 0x3000 || cp == 0xfeff
            || (cp >= 0xe000 && cp <= 0xf8ff) || (cp >= 0xfff9 && cp <= 0xfffb))
      {
         if (cp <= 0xff)
            sprintf(tmp, "\\x%02lx", cp);
         else
            sprintf(tmp, "\\u%04lx", cp);
         ck_sb_puts(sb, tmp);
      }
      else
         ck_sb_add(sb, (const char*)p, n);
      p += n;
   }
   ck_sb_putc(sb, quote);
}


/* Comments and string bodies blanked, as the GPU driver checkers read
 * a file: a block comment becomes spaces with its line breaks kept (an
 * unclosed one runs to the end), a line comment spaces, and a string
 * literal its two quotes around as many spaces as it had bytes inside -
 * a line break in it included. Returns new text. */
CK_FN char *ck_blank_code(const char *t, size_t n, size_t *out_len)
{
   struct ck_sb out = {0};
   size_t i = 0;
   ck_sb_reserve(&out, n + 2);
   while (i < n)
   {
      size_t j;
      if (t[i] == '/' && i + 1 < n && t[i + 1] == '*')
      {
         for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
         j = j + 1 < n ? j + 2 : n;
         for (; i < j; i++)
            ck_sb_putc(&out, t[i] == '\n' ? '\n' : ' ');
      }
      else if (t[i] == '/' && i + 1 < n && t[i + 1] == '/')
      {
         for (j = i; j < n && t[j] != '\n'; j++)
            ck_sb_putc(&out, ' ');
         i = j;
      }
      else if (t[i] == '"')
      {
         size_t k;
         for (j = i + 1; j < n && t[j] != '"'; j++)
         {
            if (t[j] == '\\')
               j++;
         }
         ck_sb_putc(&out, '"');
         for (k = i + 1; k < j; k++)
            ck_sb_putc(&out, ' ');
         ck_sb_putc(&out, '"');
         i = j + 1;
      }
      else
         ck_sb_putc(&out, t[i++]);
   }
   ck_sb_puts(&out, "");
   if (out_len)
      *out_len = out.len;
   return out.p;
}

/* The name a function definition line gives, as the GPU driver
 * checkers find it:
 *   ^[A-Za-z_][A-Za-z_0-9 \t*]*?\b([A-Za-z_][A-Za-z_0-9]*)\s*\(
 * Returns 0 when the line has none. */
CK_FN int ck_def_name(const char *b, const char *e, const char **nb,
      const char **ne)
{
   const char *p;
   if (b >= e || !CK_IS_IDENT0(*b))
      return 0;
   for (p = b + 1; p < e; p++)
   {
      if (CK_IS_IDENT0(*p) && !CK_IS_WORD(p[-1]))
      {
         const char *r, *q;
         for (r = p; r < e && CK_IS_WORD(*r); r++);
         for (q = r; q < e && CK_IS_SPACE(*q); q++);
         if (q < e && *q == '(')
         {
            *nb = p;
            *ne = r;
            return 1;
         }
      }
      if (!(CK_IS_WORD(*p) || *p == ' ' || *p == '\t' || *p == '*'))
         return 0;
   }
   return 0;
}


#ifdef _WIN32
#define CK_POPEN  _popen
#define CK_PCLOSE _pclose
#else
#define CK_POPEN  popen
#define CK_PCLOSE pclose
#endif

/* Append s to a shell command line as one quoted word. */
CK_FN void ck_sb_shq(struct ck_sb *sb, const char *s)
{
#ifdef _WIN32
   ck_sb_putc(sb, '"');
   for (; *s; s++)
   {
      if (*s == '"')
         ck_sb_putc(sb, '\\');
      ck_sb_putc(sb, *s);
   }
   ck_sb_putc(sb, '"');
#else
   ck_sb_putc(sb, '\'');
   for (; *s; s++)
   {
      if (*s == '\'')
         ck_sb_puts(sb, "'\\''");
      else
         ck_sb_putc(sb, *s);
   }
   ck_sb_putc(sb, '\'');
#endif
}

/* Run a shell command; its stdout, NUL-terminated, and its exit status
 * (-1 when it could not be run). stderr passes through. */
CK_FN char *ck_capture(const char *cmd, size_t *len, int *status)
{
   struct ck_sb out = {0};
   char buf[8192];
   size_t n;
   int rc;
   FILE *f;
   fflush(stdout);
   fflush(stderr);
   if (!(f = CK_POPEN(cmd, "r")))
   {
      *status = -1;
      return NULL;
   }
   while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
      ck_sb_add(&out, buf, n);
   rc = CK_PCLOSE(f);
#if !defined(_WIN32) && defined(WIFEXITED)
   if (rc != -1)
      rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 128;
#endif
   *status = rc;
   ck_sb_add(&out, "", 0);
   if (len)
      *len = out.len;
   return out.p ? out.p : ck_strdup("");
}

#endif

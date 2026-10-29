/* vita-pack-vpk: packs a Vita application into a VPK - a zip with the
 * param.sfo as sce_sys/param.sfo, the executable as eboot.bin and any
 * other files or directories where they are asked to go - as vitasdk's
 * vita-pack-vpk does.
 *
 * Usage: vita-pack-vpk [OPTIONS] output.vpk
 *   -s, --sfo=param.sfo     the param.sfo
 *   -b, --eboot=eboot.bin   the executable
 *   -a, --add src=dst       a file or directory, at dst in the VPK
 *   -h, --help              this help
 * Each entry is deflated, or stored when that is no smaller. Nothing is
 * written unless every file goes in. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <boolean.h>
#include <encodings/crc32.h>
#include <encodings/deflate.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#define DEFAULT_OUTPUT "output.vpk"

#define ZIP_LOCAL      0x04034b50UL
#define ZIP_CENTRAL    0x02014b50UL
#define ZIP_END        0x06054b50UL
#define ZIP_VERSION    20
#define ZIP_MADE_BY    ((3 << 8) | ZIP_VERSION)  /* Unix, 2.0 */
#define ZIP_STORE      0
#define ZIP_DEFLATE    8
#define LOCAL_SIZE     30
#define CENTRAL_SIZE   46
#define END_SIZE       22

typedef struct
{
   char          *name;
   unsigned long  offset;    /* of its local header */
   unsigned long  crc;
   unsigned long  csize;
   unsigned long  usize;
   unsigned long  mode;
   unsigned       dos_time;
   unsigned       dos_date;
   unsigned       method;
} vpk_entry_t;

typedef struct
{
   unsigned char *data;
   size_t         len;
   size_t         cap;
   vpk_entry_t   *entries;
   size_t         count;
   size_t         entries_cap;
} vpk_t;

static bool vpk_reserve(vpk_t *v, size_t more)
{
   unsigned char *p;
   size_t cap = v->cap ? v->cap : 65536;
   if (v->len + more <= v->cap)
      return true;
   while (cap < v->len + more)
      cap *= 2;
   if (!(p = (unsigned char*)realloc(v->data, cap)))
      return false;
   v->data = p;
   v->cap  = cap;
   return true;
}

static void put16(unsigned char *p, unsigned long v)
{
   p[0] = (unsigned char)(v & 0xFF);
   p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void put32(unsigned char *p, unsigned long v)
{
   p[0] = (unsigned char)(v & 0xFF);
   p[1] = (unsigned char)((v >> 8) & 0xFF);
   p[2] = (unsigned char)((v >> 16) & 0xFF);
   p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static void dos_time(time_t t, unsigned *time_out, unsigned *date_out)
{
   struct tm *tm = localtime(&t);
   if (!tm || tm->tm_year < 80)
   {
      *time_out = 0;
      *date_out = (1 << 5) | 1;                  /* 1980-01-01 */
      return;
   }
   *time_out = (unsigned)((tm->tm_hour << 11) | (tm->tm_min << 5)
         | (tm->tm_sec / 2));
   *date_out = (unsigned)(((tm->tm_year - 80) << 9)
         | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
}

static unsigned char *read_file(const char *path, size_t *len)
{
   unsigned char *buf;
   long size;
   FILE *fp = fopen(path, "rb");

   if (!fp)
      return NULL;
   if (fseek(fp, 0, SEEK_END) || (size = ftell(fp)) < 0
         || fseek(fp, 0, SEEK_SET))
   {
      fclose(fp);
      return NULL;
   }
   /* One byte more, so an empty file still has a buffer */
   if (!(buf = (unsigned char*)malloc((size_t)size + 1)))
   {
      fclose(fp);
      return NULL;
   }
   if (fread(buf, 1, (size_t)size, fp) != (size_t)size)
   {
      fclose(fp);
      free(buf);
      return NULL;
   }
   fclose(fp);
   *len = (size_t)size;
   return buf;
}

/* Deflated into the VPK straight after its local header, as long as
 * that comes out smaller; 0 when it does not. */
static size_t deflate_into(unsigned char *out, const unsigned char *in,
      size_t len)
{
   size_t got = 0;
   int    ret;
   void  *z;

   if (!len || !(z = rdeflate_new(6, -15)))
      return 0;
   rdeflate_set_in(z, in, len);
   rdeflate_set_out(z, out, len);
   rdeflate_finish(z);
   for (;;)
   {
      size_t read = 0, wrote = 0;
      ret  = rdeflate_process(z, &read, &wrote);
      got += wrote;
      if (ret != RDEFLATE_PROCESS_NEXT || got >= len)
         break;
      if (!read && !wrote)
         break;
   }
   rdeflate_free(z);
   return (ret == RDEFLATE_PROCESS_END && got < len) ? got : 0;
}

static bool add_file(vpk_t *v, const char *src, const char *dst,
      const struct stat *st)
{
   unsigned char *data, *hdr;
   size_t len = 0, name_len = strlen(dst), csize, i;
   vpk_entry_t *e;

   for (i = 0; i < v->count; i++)
   {
      if (!strcmp(v->entries[i].name, dst))
      {
         fprintf(stderr, "Error: cannot add '%s' to zip as '%s': "
               "File already exists\n", src, dst);
         return false;
      }
   }

   if (!(data = read_file(src, &len)))
   {
      fprintf(stderr, "Error: cannot read '%s': %s\n", src, strerror(errno));
      return false;
   }
   if (len > 0xFFFFFFFFUL || v->len + LOCAL_SIZE + name_len + len
         > 0xFFFFFFFFUL)
   {
      fprintf(stderr, "Error: '%s' does not fit in a VPK\n", src);
      free(data);
      return false;
   }

   if (v->count == v->entries_cap)
   {
      size_t cap = v->entries_cap ? v->entries_cap * 2 : 16;
      vpk_entry_t *p = (vpk_entry_t*)realloc(v->entries, cap * sizeof(*p));
      if (!p)
      {
         free(data);
         return false;
      }
      v->entries     = p;
      v->entries_cap = cap;
   }
   if (!vpk_reserve(v, LOCAL_SIZE + name_len + len))
   {
      free(data);
      return false;
   }

   e         = &v->entries[v->count];
   e->offset = (unsigned long)v->len;
   e->usize  = (unsigned long)len;
   e->crc    = encoding_crc32(0, data, len);
#ifdef _WIN32
   e->mode   = 0100666UL;
#else
   e->mode   = 0100000UL | ((unsigned long)st->st_mode & 07777UL);
#endif
   dos_time(st->st_mtime, &e->dos_time, &e->dos_date);
   if (!(e->name = (char*)malloc(name_len + 1)))
   {
      free(data);
      return false;
   }
   memcpy(e->name, dst, name_len + 1);

   csize = deflate_into(v->data + v->len + LOCAL_SIZE + name_len, data, len);
   if (csize)
      e->method = ZIP_DEFLATE;
   else
   {
      e->method = ZIP_STORE;
      csize     = len;
      if (len)
         memcpy(v->data + v->len + LOCAL_SIZE + name_len, data, len);
   }
   e->csize = (unsigned long)csize;
   free(data);

   hdr = v->data + v->len;
   put32(hdr,      ZIP_LOCAL);
   put16(hdr + 4,  ZIP_VERSION);
   put16(hdr + 6,  0);
   put16(hdr + 8,  e->method);
   put16(hdr + 10, e->dos_time);
   put16(hdr + 12, e->dos_date);
   put32(hdr + 14, e->crc);
   put32(hdr + 18, e->csize);
   put32(hdr + 22, e->usize);
   put16(hdr + 26, (unsigned long)name_len);
   put16(hdr + 28, 0);
   memcpy(hdr + LOCAL_SIZE, dst, name_len);

   v->len += LOCAL_SIZE + name_len + csize;
   v->count++;
   return true;
}

static int compare_names(const void *a, const void *b)
{
   return strcmp(*(char* const*)a, *(char* const*)b);
}

/* A directory's names, sorted so a VPK comes out the same each time */
static char **list_dir(const char *path, size_t *count)
{
   char  **names = NULL;
   size_t  n = 0, cap = 0;
#ifdef _WIN32
   WIN32_FIND_DATAA fd;
   HANDLE h;
   char pattern[MAX_PATH];
   _snprintf(pattern, sizeof(pattern), "%s\\*", path);
   pattern[sizeof(pattern) - 1] = '\0';
   if ((h = FindFirstFileA(pattern, &fd)) == INVALID_HANDLE_VALUE)
      return NULL;
   do
   {
      const char *name = fd.cFileName;
#else
   struct dirent *de;
   DIR *dir = opendir(path);
   if (!dir)
      return NULL;
   while ((de = readdir(dir)))
   {
      const char *name = de->d_name;
#endif
      char *copy;
      if (!strcmp(name, ".") || !strcmp(name, ".."))
         continue;
      if (n == cap)
      {
         char **p;
         cap = cap ? cap * 2 : 16;
         if (!(p = (char**)realloc(names, cap * sizeof(*p))))
            break;
         names = p;
      }
      if (!(copy = (char*)malloc(strlen(name) + 1)))
         break;
      strcpy(copy, name);
      names[n++] = copy;
#ifdef _WIN32
   } while (FindNextFileA(h, &fd));
   FindClose(h);
#else
   }
   closedir(dir);
#endif
   if (n)
      qsort(names, n, sizeof(*names), compare_names);
   *count = n;
   if (!names)
      names = (char**)malloc(sizeof(*names));
   return names;
}

static bool add_path(vpk_t *v, const char *src, const char *dst)
{
   struct stat st;
   char  **names;
   size_t  count = 0, i;
   bool    ok = true;

   if (stat(src, &st))
   {
      fprintf(stderr, "Error: cannot stat '%s': %s\n", src, strerror(errno));
      return false;
   }
   if (S_ISREG(st.st_mode))
      return add_file(v, src, dst, &st);
   if (!S_ISDIR(st.st_mode))
   {
      fprintf(stderr, "Error: unsupported file type for '%s'\n", src);
      return false;
   }

   if (!(names = list_dir(src, &count)))
   {
      fprintf(stderr, "Error: cannot open directory '%s': %s\n", src,
            strerror(errno));
      return false;
   }
   for (i = 0; i < count; i++)
   {
      size_t sl = strlen(src) + strlen(names[i]) + 2;
      size_t dl = strlen(dst) + strlen(names[i]) + 2;
      char  *s  = (char*)malloc(sl);
      char  *d  = (char*)malloc(dl);
      if (ok && s && d)
      {
         sprintf(s, "%s/%s", src, names[i]);
         sprintf(d, "%s/%s", dst, names[i]);
         ok = add_path(v, s, d);
      }
      else
         ok = false;
      free(s);
      free(d);
      free(names[i]);
   }
   free(names);
   return ok;
}

static bool write_vpk(vpk_t *v, const char *path)
{
   size_t i, dir_at = v->len, dir_len = 0, written;
   FILE  *fp;

   for (i = 0; i < v->count; i++)
      dir_len += CENTRAL_SIZE + strlen(v->entries[i].name);
   if (!vpk_reserve(v, dir_len + END_SIZE))
      return false;

   for (i = 0; i < v->count; i++)
   {
      const vpk_entry_t *e = &v->entries[i];
      size_t name_len      = strlen(e->name);
      unsigned char *p     = v->data + v->len;

      memset(p, 0, CENTRAL_SIZE);
      put32(p,      ZIP_CENTRAL);
      put16(p + 4,  ZIP_MADE_BY);
      put16(p + 6,  ZIP_VERSION);
      put16(p + 10, e->method);
      put16(p + 12, e->dos_time);
      put16(p + 14, e->dos_date);
      put32(p + 16, e->crc);
      put32(p + 20, e->csize);
      put32(p + 24, e->usize);
      put16(p + 28, (unsigned long)name_len);
      put32(p + 38, e->mode << 16);
      put32(p + 42, e->offset);
      memcpy(p + CENTRAL_SIZE, e->name, name_len);
      v->len += CENTRAL_SIZE + name_len;
   }

   {
      unsigned char *p = v->data + v->len;
      memset(p, 0, END_SIZE);
      put32(p,      ZIP_END);
      put16(p + 8,  (unsigned long)v->count);
      put16(p + 10, (unsigned long)v->count);
      put32(p + 12, (unsigned long)dir_len);
      put32(p + 16, (unsigned long)dir_at);
      v->len += END_SIZE;
   }

   if (!(fp = fopen(path, "wb")))
   {
      printf("Error creating: '%s': %s\n", path, strerror(errno));
      return false;
   }
   written = fwrite(v->data, 1, v->len, fp);
   if (fclose(fp) || written != v->len)
   {
      printf("Error creating: '%s': %s\n", path, strerror(errno));
      remove(path);
      return false;
   }
   return true;
}

static void usage(const char *arg)
{
   printf("Usage:\n\t%s [OPTIONS] output.vpk\n\n"
      "  -s, --sfo=param.sfo     sets the param.sfo file\n"
      "  -b, --eboot=eboot.bin   sets the eboot.bin file\n"
      "  -a, --add src=dst       adds the file or directory src to the vpk as dst\n"
      "  -h, --help              displays this help and exit\n"
      , arg);
}

/* A long option, or an unambiguous start of one */
static int long_option(const char *arg, size_t len)
{
   static const char *names[] = { "sfo", "eboot", "add", "help" };
   static const int   codes[] = { 's', 'b', 'a', 'h' };
   int i, found = 0, matches = 0;
   for (i = 0; i < 4; i++)
   {
      if (!strncmp(names[i], arg, len))
      {
         if (strlen(names[i]) == len)
            return codes[i];
         found = codes[i];
         matches++;
      }
   }
   return matches == 1 ? found : 0;
}

int main(int argc, char **argv)
{
   const char  *sfo = NULL, *eboot = NULL, *output = NULL;
   const char **adds;
   size_t nadds = 0, i;
   int    a, opts = 1, ret = -1;
   vpk_t  v;

   if (argc < 2)
   {
      usage(argv[0]);
      return -1;
   }
   if (!(adds = (const char**)malloc((size_t)argc * sizeof(*adds))))
      return -1;

   for (a = 1; a < argc; a++)
   {
      char       *arg = argv[a];
      const char *val = NULL;
      int         opt = 0;

      if (opts && !strcmp(arg, "--"))
      {
         opts = 0;
         continue;
      }
      if (opts && arg[0] == '-' && arg[1] == '-')
      {
         const char *eq = strchr(arg + 2, '=');
         size_t len     = eq ? (size_t)(eq - (arg + 2)) : strlen(arg + 2);
         if (!(opt = long_option(arg + 2, len)))
         {
            fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], arg);
            continue;
         }
         if (eq)
            val = eq + 1;
      }
      else if (opts && arg[0] == '-' && arg[1])
      {
         opt = arg[1];
         if (opt != 's' && opt != 'b' && opt != 'a' && opt != 'h')
         {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
            continue;
         }
         if (arg[2])
            val = arg + 2;
      }
      else
      {
         if (!output)
            output = arg;
         continue;
      }

      if (opt == 'h')
      {
         usage(argv[0]);
         free(adds);
         return -1;
      }
      if (!val)
      {
         if (a + 1 == argc)
         {
            fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                  argv[0], opt);
            continue;
         }
         val = argv[++a];
      }
      if (opt == 's')
         sfo = val;
      else if (opt == 'b')
         eboot = val;
      else
      {
         /* src=dst; one without '=' or with nothing after it is ignored */
         const char *eq = strchr(val, '=');
         if (eq && eq[1])
            adds[nadds++] = val;
      }
   }

   if (!sfo)
   {
      printf(".sfo file missing.\n");
      free(adds);
      return -1;
   }
   if (!eboot)
   {
      printf(".bin file missing.\n");
      free(adds);
      return -1;
   }
   if (!output)
      output = DEFAULT_OUTPUT;

   memset(&v, 0, sizeof(v));
   if (!add_path(&v, sfo, "sce_sys/param.sfo"))
      fprintf(stderr, "Error: failed to add sfo file '%s'\n", sfo);
   else if (!add_path(&v, eboot, "eboot.bin"))
      fprintf(stderr, "Error: failed to add eboot file '%s'\n", eboot);
   else
   {
      for (i = 0; i < nadds; i++)
      {
         const char *eq  = strchr(adds[i], '=');
         size_t      len = (size_t)(eq - adds[i]);
         char       *src = (char*)malloc(len + 1);
         bool        ok;
         if (!src)
            break;
         memcpy(src, adds[i], len);
         src[len] = '\0';
         ok = add_path(&v, src, eq + 1);
         if (!ok)
            fprintf(stderr, "Error: failed to add additional file '%s' as '%s'\n",
                  src, eq + 1);
         free(src);
         if (!ok)
            break;
      }
      if (i == nadds && write_vpk(&v, output))
         ret = 0;
   }

   for (i = 0; i < v.count; i++)
      free(v.entries[i].name);
   free(v.entries);
   free(v.data);
   free(adds);
   return ret;
}

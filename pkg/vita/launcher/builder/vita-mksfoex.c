/* vita-mksfoex: writes the param.sfo a Vita application's bubble needs,
 * as vitasdk's vita-mksfoex of July 2016 does - the same default keys,
 * in the same order, with the title as TITLE and STITLE.
 *
 * Usage: vita-mksfoex [options] TITLE output.sfo
 *   -d, --dword NAME=VALUE   a 32-bit value (C notation: 0x.., 0..)
 *   -s, --string NAME=STR    a string
 *   -e, --empty              accepted and ignored, as it always was
 * A key already there takes the new value; a new key goes after the
 * others. Options may come anywhere; "--" ends them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSF_MAGIC    0x46535000UL
#define PSF_VERSION  0x00000101UL

#define PSF_TYPE_STR 2
#define PSF_TYPE_VAL 4

#define HEADER_SIZE  20
#define ENTRY_SIZE   16
#define MAX_ENTRIES  256

typedef struct
{
   const char   *name;
   const char   *data;   /* PSF_TYPE_STR */
   unsigned long value;  /* the value, or a string's fixed size (0: fit) */
   int           type;
} sfo_entry_t;

static const sfo_entry_t sfo_defaults[] =
{
   { "APP_VER",             "00.00",     0,      PSF_TYPE_STR },
   { "ATTRIBUTE",           NULL,        0x8000, PSF_TYPE_VAL },
   { "ATTRIBUTE2",          NULL,        0,      PSF_TYPE_VAL },
   { "ATTRIBUTE_MINOR",     NULL,        0x10,   PSF_TYPE_VAL },
   { "BOOT_FILE",           "",          32,     PSF_TYPE_STR },
   { "CATEGORY",            "gd",        0,      PSF_TYPE_STR },
   { "CONTENT_ID",          "",          48,     PSF_TYPE_STR },
   { "EBOOT_APP_MEMSIZE",   NULL,        0,      PSF_TYPE_VAL },
   { "EBOOT_ATTRIBUTE",     NULL,        0,      PSF_TYPE_VAL },
   { "EBOOT_PHY_MEMSIZE",   NULL,        0,      PSF_TYPE_VAL },
   { "LAREA_TYPE",          NULL,        0,      PSF_TYPE_VAL },
   { "NP_COMMUNICATION_ID", "",          16,     PSF_TYPE_STR },
   { "PARENTAL_LEVEL",      NULL,        0,      PSF_TYPE_VAL },
   { "PSP2_DISP_VER",       "00.000",    0,      PSF_TYPE_STR },
   { "PSP2_SYSTEM_VER",     NULL,        0,      PSF_TYPE_VAL },
   { "STITLE",              "Homebrew",  52,     PSF_TYPE_STR },
   { "TITLE",               "Homebrew",  0x80,   PSF_TYPE_STR },
   { "TITLE_ID",            "ABCD99999", 0,      PSF_TYPE_STR },
   { "VERSION",             "00.00",     0,      PSF_TYPE_STR }
};

static sfo_entry_t entries[MAX_ENTRIES];
static int         entry_count;

static sfo_entry_t *find_entry(const char *name)
{
   int i;
   for (i = 0; i < entry_count; i++)
      if (!strcmp(entries[i].name, name))
         return &entries[i];
   return NULL;
}

static void usage(void)
{
   fprintf(stderr, "Usage: mksfoex [options] TITLE output.sfo\n");
   fprintf(stderr, "Options:\n");
   fprintf(stderr, "-d NAME=VALUE - Add a new DWORD value\n");
   fprintf(stderr, "-s NAME=STR   - Add a new string value\n");
}

/* NAME=VALUE: the value of an existing key, or a new key. A string
 * without '=' is reported and skipped; a dword without one is an
 * error. */
static int add_option(char *opt, int type)
{
   sfo_entry_t *e;
   char *eq = strchr(opt, '=');

   if (!eq)
   {
      fprintf(stderr, "Invalid option (no =)\n");
      return type == PSF_TYPE_STR;
   }
   *eq++ = '\0';

   if (!(e = find_entry(opt)))
   {
      if (entry_count == MAX_ENTRIES)
      {
         fprintf(stderr, "Maximum options reached\n");
         return 0;
      }
      e        = &entries[entry_count++];
      e->name  = opt;
      e->data  = NULL;
      e->value = 0;
      e->type  = type;
   }

   if (type == PSF_TYPE_STR)
      e->data  = eq;
   else
      e->value = strtoul(eq, NULL, 0);
   return 1;
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

/* Header, entries, key names (padded to 4) and values, in one buffer. */
static int write_sfo(const char *path)
{
   int i;
   size_t keys_len = 0, data_len = 0, keys_pad, size, at, key_at, data_at;
   unsigned char *out;
   FILE *fp;

   for (i = 0; i < entry_count; i++)
   {
      const sfo_entry_t *e = &entries[i];
      keys_len += strlen(e->name) + 1;
      if (e->type == PSF_TYPE_VAL)
         data_len += 4;
      else
      {
         size_t len = e->data ? strlen(e->data) + 1 : 0;
         if (e->value && len > e->value)
         {
            fprintf(stderr, "%s is longer than its %lu bytes\n",
                  e->name, e->value);
            return 0;
         }
         data_len += e->value ? e->value : ((len + 3) & ~(size_t)3);
      }
   }
   keys_pad = (keys_len + 3) & ~(size_t)3;
   size     = HEADER_SIZE + (size_t)entry_count * ENTRY_SIZE
      + keys_pad + data_len;

   if (!(out = (unsigned char*)calloc(size, 1)))
      return 0;

   put32(out,      PSF_MAGIC);
   put32(out + 4,  PSF_VERSION);
   put32(out + 8,  HEADER_SIZE + (unsigned long)entry_count * ENTRY_SIZE);
   put32(out + 12, HEADER_SIZE + (unsigned long)entry_count * ENTRY_SIZE
         + keys_pad);
   put32(out + 16, (unsigned long)entry_count);

   key_at  = 0;
   data_at = 0;
   for (i = 0; i < entry_count; i++)
   {
      const sfo_entry_t *e = &entries[i];
      unsigned char *ent   = out + HEADER_SIZE + (size_t)i * ENTRY_SIZE;
      unsigned char *data  = out + HEADER_SIZE
         + (size_t)entry_count * ENTRY_SIZE + keys_pad + data_at;
      size_t name_len      = strlen(e->name) + 1;

      put16(ent, (unsigned long)key_at);
      ent[2] = 4;                                  /* alignment */
      ent[3] = (unsigned char)e->type;
      put32(ent + 12, (unsigned long)data_at);

      memcpy(out + HEADER_SIZE + (size_t)entry_count * ENTRY_SIZE + key_at,
            e->name, name_len);
      key_at += name_len;

      if (e->type == PSF_TYPE_VAL)
      {
         put32(ent + 4, 4);
         put32(ent + 8, 4);
         put32(data, e->value);
         data_at += 4;
      }
      else
      {
         size_t len   = e->data ? strlen(e->data) + 1 : 0;
         size_t total = e->value ? e->value : ((len + 3) & ~(size_t)3);
         put32(ent + 4, (unsigned long)len);
         put32(ent + 8, (unsigned long)total);
         if (len)
            memcpy(data, e->data, len);
         data_at += total;
      }
   }

   at = 0;
   if (!(fp = fopen(path, "wb")))
   {
      fprintf(stderr, "Cannot open filename %s\n", path);
      free(out);
      return 0;
   }
   at = fwrite(out, 1, size, fp);
   fclose(fp);
   free(out);
   return at == size;
}

/* A long option, or an unambiguous start of one: 'd', 's', 'e' or 0 */
static int long_option(const char *arg, size_t len)
{
   static const char *names[] = { "dword", "string", "empty" };
   static const int   codes[] = { 'd', 's', 'e' };
   int i, found = 0, matches = 0;
   for (i = 0; i < 3; i++)
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
   const char *pos[2];
   int npos = 0, i, opts = 1;

   for (i = 0; i < (int)(sizeof(sfo_defaults) / sizeof(sfo_defaults[0])); i++)
      entries[entry_count++] = sfo_defaults[i];

   for (i = 1; i < argc; i++)
   {
      char *arg = argv[i];

      if (opts && arg[0] == '-' && arg[1] == '-' && !arg[2])
         opts = 0;
      else if (opts && arg[0] == '-' && arg[1] == '-')
      {
         char  *eq  = strchr(arg + 2, '=');
         size_t len = eq ? (size_t)(eq - (arg + 2)) : strlen(arg + 2);
         int    opt = long_option(arg + 2, len);
         char  *val = eq ? eq + 1 : NULL;

         if (!opt)
         {
            fprintf(stderr, "%s: unrecognized option `%s'\n", argv[0], arg);
            continue;
         }
         if (opt == 'e')
            continue;
         if (!val)
         {
            if (i + 1 == argc)
            {
               fprintf(stderr, "%s: option `%s' requires an argument\n",
                     argv[0], arg);
               continue;
            }
            val = argv[++i];
         }
         if (!add_option(val, opt == 'd' ? PSF_TYPE_VAL : PSF_TYPE_STR))
         {
            usage();
            return 1;
         }
      }
      else if (opts && arg[0] == '-' && arg[1])
      {
         /* Short options, bundled as getopt allows */
         char *c;
         for (c = arg + 1; *c; c++)
         {
            char *val;
            if (*c == 'e')
               continue;
            if (*c != 'd' && *c != 's')
            {
               fprintf(stderr, "%s: invalid option -- %c\n", argv[0], *c);
               continue;
            }
            if (c[1])
               val = c + 1;
            else if (i + 1 < argc)
               val = argv[++i];
            else
            {
               fprintf(stderr, "%s: option requires an argument -- %c\n",
                     argv[0], *c);
               break;
            }
            if (!add_option(val, *c == 'd' ? PSF_TYPE_VAL : PSF_TYPE_STR))
            {
               usage();
               return 1;
            }
            break;
         }
      }
      else if (npos < 2)
         pos[npos++] = arg;
   }

   if (npos < 2)
   {
      usage();
      return 1;
   }

   find_entry("TITLE")->data  = pos[0];
   find_entry("STITLE")->data = pos[0];

   return write_sfo(pos[1]) ? 0 : 1;
}

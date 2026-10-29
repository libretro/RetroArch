/* pkg/vita/launcher/builder's host tools, which build.bat runs to make a
 * Vita bubble: vita-mksfoex and vita-pack-vpk.
 *
 * vita-mksfoex must write the param.sfo vitasdk's of July 2016 writes,
 * byte for byte, for every way it can be asked: the size and CRC-32 of
 * each case below are that tool's output for the same arguments. A title
 * longer than STITLE holds, and a dword without '=', are refused with no
 * file written.
 *
 * vita-pack-vpk must give a zip that libretro-common's reader takes
 * back: param.sfo and eboot.bin first, then what -a adds in order, a
 * directory's files in name order, each member whole - deflated when
 * that is smaller, stored when not. A missing file or two members under
 * one name fail it with no VPK written; an -a without '=' is ignored. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define EXE ".exe"
#define RUN ""
#else
#include <sys/wait.h>
#include <unistd.h>
#define make_dir(p) mkdir((p), 0755)
#define EXE ""
#define RUN "./"
#endif

#include <encodings/crc32.h>
#include <zip/rzip_archive.h>

#define MKSFO   RUN "vita_mksfoex_tool" EXE
#define PACKVPK RUN "vita_pack_vpk_tool" EXE
#define SFO_OUT "vlt_out.sfo"
#define VPK_OUT "vlt_out.vpk"

static int failures;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   failures++; } } while (0)

/* The status a tool exited with, its low byte as POSIX reports it: the
 * -1 vita-pack-vpk returns comes back as 255 on both. */
static int exit_code(int rc)
{
#ifdef _WIN32
   return rc & 0xFF;
#else
   return (rc != -1 && WIFEXITED(rc)) ? WEXITSTATUS(rc) : -1000;
#endif
}

static int run(const char *cmd)
{
   char line[1024];
#ifdef _WIN32
   sprintf(line, "%s >NUL 2>&1", cmd);
#else
   sprintf(line, "%s >/dev/null 2>&1", cmd);
#endif
   return exit_code(system(line));
}

static unsigned char *read_all(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   unsigned char *buf;
   long size;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   size = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (unsigned char*)malloc((size_t)size + 1);
   if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   *len = (size_t)size;
   return buf;
}

static int write_all(const char *path, const void *data, size_t len)
{
   FILE *f = fopen(path, "wb");
   int ok;
   if (!f)
      return 0;
   ok = fwrite(data, 1, len, f) == len;
   return fclose(f) == 0 && ok;
}

static int exists(const char *path)
{
   FILE *f = fopen(path, "rb");
   if (f)
      fclose(f);
   return f != NULL;
}

typedef struct
{
   const char   *args;
   unsigned long size;
   unsigned long crc;
} sfo_case_t;

/* What vitasdk's 2016 vita-mksfoex writes for each */
static const sfo_case_t sfo_cases[] =
{
   { "-s TITLE_ID=RAXXXXXXX \"Launcher\"", 912, 0xd4601a3aUL },
   { "-s TITLE_ID=ABCD12345 \"My Bubble Name\"", 912, 0x06cf2f61UL },
   { "-d PARENTAL_LEVEL=5 -s NEWKEY=hello -d NEWVAL=0x1234 "
     "-s APP_VER=01.50 -d ZERO=010 \"T\"", 992, 0x2df1deecUL },
   { "-s TITLE_ID=LONG00001 "
     "\"Fifty-one characters of title text here, exactly!!\"",
     912, 0x0fae55c9UL },
   { "\"Perm Title\" -s TITLE_ID=PERM00001", 912, 0x5b8cc6a7UL },
   { "--string=TITLE_ID=LOPT00001 --dword ATTRIBUTE=0 "
     "--string CATEGORY=gp \"LongOpt\"", 912, 0x33ccc2ccUL },
   { "-e \"Empty\"", 912, 0xf672bc2eUL },
   { "-s BADOPTION -s TITLE_ID=NOEQ00001 \"NoEq\"", 912, 0x26c8e593UL },
   { "-s TITLE_ID=DASH00001 -- \"-dash title\"", 912, 0x8beedbc9UL },
   { "-s NP_COMMUNICATION_ID= -s TITLE_ID= \"E\"", 904, 0xea2f80e8UL }
};

static void check_mksfoex(void)
{
   char cmd[512];
   size_t i, len;
   unsigned char *sfo;

   for (i = 0; i < sizeof(sfo_cases) / sizeof(sfo_cases[0]); i++)
   {
      remove(SFO_OUT);
      /* The output name last, as build.bat gives it */
      sprintf(cmd, "%s %s %s", MKSFO, sfo_cases[i].args, SFO_OUT);
      CHECK(run(cmd) == 0);
      sfo = read_all(SFO_OUT, &len);
      CHECK(sfo != NULL);
      if (!sfo)
         continue;
      if (len != sfo_cases[i].size
            || encoding_crc32(0, sfo, len) != sfo_cases[i].crc)
      {
         fprintf(stderr, "case %u (%s): %u bytes, CRC %08lx; want %lu, %08lx\n",
               (unsigned)i, sfo_cases[i].args, (unsigned)len,
               (unsigned long)encoding_crc32(0, sfo, len),
               sfo_cases[i].size, sfo_cases[i].crc);
         failures++;
      }
      free(sfo);
   }

   /* Refused, with nothing written */
   remove(SFO_OUT);
   CHECK(run(MKSFO " -d BADDWORD \"T\" " SFO_OUT) == 1);
   CHECK(!exists(SFO_OUT));
   CHECK(run(MKSFO " \"A title that is far too long for the fifty-two "
            "byte STITLE slot\" " SFO_OUT) == 1);
   CHECK(!exists(SFO_OUT));
   CHECK(run(MKSFO " \"OnlyTitle\"") == 1);
   CHECK(run(MKSFO) == 1);
}

#define EBOOT_LEN 70000
#define ICON_LEN  13000

typedef struct
{
   const char *name;
   const char *src;
   int         method;  /* -1: either */
} vpk_member_t;

static const vpk_member_t vpk_members[] =
{
   { "sce_sys/param.sfo",        "vlt_in/param.sfo",  -1 },
   { "eboot.bin",                "vlt_in/eboot.bin",  RZIP_METHOD_STORED },
   { "sce_sys/icon0.png",        "vlt_in/icon0.png",  RZIP_METHOD_DEFLATE },
   { "rom.txt",                  "vlt_in/rom.txt",    RZIP_METHOD_STORED },
   { "extra/a.txt",              "vlt_in/dir/a.txt",  -1 },
   { "extra/b.txt",              "vlt_in/dir/b.txt",  -1 },
   { "extra/sub/c.txt",          "vlt_in/dir/sub/c.txt", -1 }
};

#define VPK_ADDS " -a vlt_in/icon0.png=sce_sys/icon0.png" \
                 " -a vlt_in/rom.txt=rom.txt -a vlt_in/dir=extra"

static void make_inputs(void)
{
   static unsigned char eboot[EBOOT_LEN];
   static char icon[ICON_LEN];
   unsigned long x = 12345;
   size_t i, len;
   unsigned char *sfo;

   make_dir("vlt_in");
   make_dir("vlt_in/dir");
   make_dir("vlt_in/dir/sub");

   /* What deflate cannot shrink, and what it can */
   for (i = 0; i < EBOOT_LEN; i++)
   {
      x = (x * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
      eboot[i] = (unsigned char)(x >> 16);
   }
   for (i = 0; i < ICON_LEN; i++)
      icon[i] = (char)('a' + (i / 7) % 26);

   remove(SFO_OUT);
   run(MKSFO " -s TITLE_ID=RAXXXXXXX \"Launcher\" " SFO_OUT);
   sfo = read_all(SFO_OUT, &len);
   CHECK(sfo && write_all("vlt_in/param.sfo", sfo, len));
   free(sfo);
   remove(SFO_OUT);

   CHECK(write_all("vlt_in/eboot.bin", eboot, EBOOT_LEN));
   CHECK(write_all("vlt_in/icon0.png", icon, ICON_LEN));
   CHECK(write_all("vlt_in/rom.txt", "", 0));
   /* Written out of name order */
   CHECK(write_all("vlt_in/dir/sub/c.txt", "ccc", 3));
   CHECK(write_all("vlt_in/dir/b.txt", "bb", 2));
   CHECK(write_all("vlt_in/dir/a.txt", "a", 1));
}

static void remove_inputs(void)
{
   remove("vlt_in/param.sfo");
   remove("vlt_in/eboot.bin");
   remove("vlt_in/icon0.png");
   remove("vlt_in/rom.txt");
   remove("vlt_in/dir/sub/c.txt");
   remove("vlt_in/dir/b.txt");
   remove("vlt_in/dir/a.txt");
   rmdir("vlt_in/dir/sub");
   rmdir("vlt_in/dir");
   rmdir("vlt_in");
}

static void check_vpk(const char *path, size_t expect)
{
   unsigned char *vpk;
   size_t len, i;
   rzip_archive_t *a = NULL;

   vpk = read_all(path, &len);
   CHECK(vpk != NULL);
   if (!vpk)
      return;
   CHECK(rzip_archive_open(&a, vpk, len, NULL, NULL) == RZIP_OK);
   if (!a)
   {
      free(vpk);
      return;
   }
   CHECK(rzip_archive_num_entries(a) == expect);

   for (i = 0; i < expect && i < rzip_archive_num_entries(a); i++)
   {
      const rzip_entry_t *e = rzip_archive_entry(a, (uint32_t)i);
      uint8_t *got = NULL;
      size_t got_len = 0, want_len = 0;
      unsigned char *want = read_all(vpk_members[i].src, &want_len);

      if (strcmp(e->name, vpk_members[i].name))
      {
         fprintf(stderr, "member %u is %s, want %s\n", (unsigned)i,
               e->name, vpk_members[i].name);
         failures++;
      }
      if (vpk_members[i].method >= 0)
         CHECK(e->method == (uint32_t)vpk_members[i].method);
      CHECK(rzip_archive_extract(a, (uint32_t)i, &got, &got_len) == RZIP_OK);
      CHECK(want && got_len == want_len
            && (!want_len || !memcmp(got, want, want_len)));
      free(got);
      free(want);
   }
   rzip_archive_close(a);
   free(vpk);
}

static void check_pack_vpk(void)
{
   make_inputs();

   remove(VPK_OUT);
   CHECK(run(PACKVPK " -s vlt_in/param.sfo -b vlt_in/eboot.bin " VPK_OUT
            VPK_ADDS) == 0);
   check_vpk(VPK_OUT, sizeof(vpk_members) / sizeof(vpk_members[0]));

   /* Long options, the output first, and an -a without '=' ignored */
   remove(VPK_OUT);
   CHECK(run(PACKVPK " " VPK_OUT " --sfo=vlt_in/param.sfo"
            " --eboot vlt_in/eboot.bin -a vlt_in/rom.txt") == 0);
   check_vpk(VPK_OUT, 2);

   /* No output named: output.vpk */
   remove("output.vpk");
   CHECK(run(PACKVPK " -s vlt_in/param.sfo -b vlt_in/eboot.bin") == 0);
   check_vpk("output.vpk", 2);
   remove("output.vpk");

   /* Failures, with no VPK left behind */
   remove(VPK_OUT);
   CHECK(run(PACKVPK " -s vlt_in/param.sfo -b vlt_in/missing.bin "
            VPK_OUT) == 255);
   CHECK(!exists(VPK_OUT));
   CHECK(run(PACKVPK " -s vlt_in/param.sfo -b vlt_in/eboot.bin " VPK_OUT
            " -a vlt_in/rom.txt=eboot.bin") == 255);
   CHECK(!exists(VPK_OUT));
   CHECK(run(PACKVPK " -b vlt_in/eboot.bin " VPK_OUT) == 255);
   CHECK(run(PACKVPK " -h") == 255);
   CHECK(run(PACKVPK) == 255);
   CHECK(!exists(VPK_OUT));

   remove(VPK_OUT);
   remove_inputs();
}

int main(void)
{
   check_mksfoex();
   check_pack_vpk();
   remove(SFO_OUT);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("vita_launcher_tools: ok\n");
   return 0;
}

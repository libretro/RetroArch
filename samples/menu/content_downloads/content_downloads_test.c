/* Lanes for menu/menu_content_downloads.c: the Content Downloader's
 * "[#]" marker must show for an entry that finished downloading even
 * when the file is gone, because auto-extract unpacked the archive and
 * deleted it (the DOOM case: "Doom (Shareware).zip" unpacks to Doom/).
 * Each lane works in a scratch folder under this directory. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "../../../menu/menu_content_downloads.h"

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, name) \
   do \
   { \
      checks++; \
      if (cond) \
         printf("  ok   %s\n", name); \
      else \
      { \
         failures++; \
         printf("  FAIL %s\n", name); \
      } \
   } while (0)

#define DIR_NAME "cd_scratch"

static void reset(void)
{
   char path[256];
   const char *names[] = { "prboom.wad", MENU_CONTENT_DOWNLOADS_RECORD };
   size_t i;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      snprintf(path, sizeof(path), DIR_NAME "/%s", names[i]);
      filestream_delete(path);
   }
   path_mkdir(DIR_NAME);
}

static bool present(const char *name)
{
   char *record = menu_content_downloads_load(DIR_NAME);
   bool   ret   = menu_content_downloads_present(DIR_NAME, record, name);
   free(record);
   return ret;
}

static int record_lines(void)
{
   int n = 0;
   char *p;
   char *record = menu_content_downloads_load(DIR_NAME);
   if (!record)
      return 0;
   for (p = record; *p; p++)
      if (*p == '\n')
         n++;
   free(record);
   return n;
}

int main(void)
{
   printf("[on disk]\n");
   reset();
   filestream_write_file(DIR_NAME "/prboom.wad", "x", 1);
   CHECK(present("prboom.wad"), "a file in the folder is marked");
   CHECK(!present("doom1.wad"), "a file never downloaded is not");

   printf("[archive unpacked and deleted]\n");
   reset();
   CHECK(menu_content_downloads_add(DIR_NAME, "Doom (Shareware).zip"),
         "recorded");
   CHECK(present("Doom (Shareware).zip"),
         "a recorded archive is marked with no file on disk");
   CHECK(!present("Doom.zip"), "a shorter name is not matched");
   CHECK(!present("(Shareware).zip"), "a suffix of a name is not matched");
   CHECK(!present("Doom (Shareware).zip.bak"), "a longer name is not matched");

   printf("[record]\n");
   reset();
   CHECK(menu_content_downloads_load(DIR_NAME) == NULL, "no record yet");
   menu_content_downloads_add(DIR_NAME, "a.zip");
   menu_content_downloads_add(DIR_NAME, "b.zip");
   menu_content_downloads_add(DIR_NAME, "a.zip");
   CHECK(record_lines() == 2, "each name once");
   CHECK(present("a.zip") && present("b.zip"), "both names marked");
   CHECK(!menu_content_downloads_add(DIR_NAME, "bad\nname.zip"),
         "a name with a line break is refused");
   CHECK(!present("bad") && !present("name.zip"),
         "and nothing of it is recorded");

   reset();
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (romm_list_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The RomM response parser (tasks/romm_list.c).
 *
 * A RomM server is configured by the user but its answers are still
 * input: a platform or file name becomes a path component, a cover
 * becomes a URL carrying the API token.  The cases below feed it a
 * trimmed real 5.4.0 reply, then names that try to leave their
 * directory and covers that point elsewhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include "../../../tasks/task_romm.h"

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

typedef struct
{
   unsigned n;
   unsigned pages;
   char path[8][512];
   char label[8][512];
} seen_t;

static bool collect(void *data, const char *path, const char *label,
      bool next_page)
{
   seen_t *s = (seen_t*)data;
   if (next_page)
      s->pages++;
   if (s->n < 8)
   {
      strlcpy(s->path[s->n], path, sizeof(s->path[0]));
      strlcpy(s->label[s->n], label ? label : "", sizeof(s->label[0]));
   }
   s->n++;
   return true;
}

static bool stop_after_one(void *data, const char *path, const char *label,
      bool next_page)
{
   (void)path; (void)label; (void)next_page;
   return ++*(unsigned*)data < 1;
}

static const char platforms[] =
   "[{\"id\":2,\"slug\":\"gba\",\"fs_slug\":\"gba\",\"rom_count\":120,"
   "\"name\":\"Game Boy Advance\",\"libretro_slug\":\"Nintendo - Game Boy Advance\","
   "\"firmware\":[{\"id\":1,\"file_name\":\"gba_bios.bin\"}],"
   "\"display_name\":\"Game Boy Advance\",\"abbreviation\":\"GBA\"},"
   /* null libretro slug: falls back to the name */
   "{\"id\":7,\"slug\":\"x\",\"fs_slug\":\"homebrew\",\"rom_count\":3,"
   "\"name\":\"Homebrew\",\"libretro_slug\":null,\"display_name\":\"My Homebrew\"},"
   /* nothing in it */
   "{\"id\":8,\"fs_slug\":\"empty\",\"rom_count\":0,\"name\":\"Empty\"},"
   /* platforms that try to leave the playlist directory */
   "{\"id\":9,\"fs_slug\":\"evil\",\"rom_count\":1,\"name\":\"Evil\","
   "\"libretro_slug\":\"../../x\"},"
   "{\"id\":10,\"fs_slug\":\"a/b\",\"rom_count\":1,\"name\":\"Evil2\"}]";

static const char roms_page1[] =
   "{\"items\":["
   "{\"id\":3,\"name\":\"Pretty Title\",\"fs_name\":\"Dummy 001.gba\","
   "\"fs_name_no_ext\":\"Dummy 001\",\"has_multiple_files\":false,"
   "\"path_cover_large\":\"/assets/romm/resources/roms/1/3/cover/big.png?ts=2026-10-16 22:26:01\","
   "\"files\":[{\"id\":9,\"file_name\":\"Dummy 001.gba\",\"name\":\"nested\"}],"
   "\"metadatum\":{\"genres\":[\"a\"]}},"
   "{\"id\":4,\"fs_name\":\"Disc.cue\",\"fs_name_no_ext\":\"Disc\","
   "\"has_multiple_files\":true,\"path_cover_large\":\"\"},"
   "{\"id\":5,\"fs_name\":\"../escape.gba\",\"fs_name_no_ext\":\"escape\","
   "\"has_multiple_files\":false},"
   "{\"id\":6,\"fs_name\":\"Elsewhere.gba\",\"fs_name_no_ext\":\"Elsewhere\","
   "\"has_multiple_files\":false,"
   "\"path_cover_large\":\"http://evil.example/steal.png\"}"
   "],\"total\":250,\"limit\":100,\"offset\":0,"
   "\"char_index\":{},\"rom_id_index\":[],\"filter_values\":{\"genres\":[]}}";

static const char roms_last[] =
   "{\"items\":[{\"id\":1,\"fs_name\":\"A.gb\",\"fs_name_no_ext\":\"A\","
   "\"has_multiple_files\":false}],\"total\":101,\"limit\":100,\"offset\":100}";

static const char roms_full[] = "{\"items\":[],\"total\":100}";

static const char unauthorized[] = "{\"detail\":\"Not authenticated\"}";

static const char odd_roms[] =
   "{\"items\":[{\"id\":5,\"fs_name\":\"pipe.gba\",\"fs_name_no_ext\":\"pipe\","
   "\"path_cover_large\":\"/assets/a|b.png\"},"
   "{\"id\":6,\"fs_name\":\"nul\\u0000.gba\",\"fs_name_no_ext\":\"x\"}],"
   "\"total\":4294967295}";

static const char noname_platform[] =
   "[{\"id\":11,\"fs_slug\":\"fsx\",\"rom_count\":3,\"name\":\"\","
   "\"libretro_slug\":\"Sys - X\"}]";

static const char odd_platforms[] =
   "[{\"id\":9,\"fs_slug\":\"slug\",\"rom_count\":2,\"name\":\"Tom/Jerry? - Remix.\","
   "\"libretro_slug\":null},"
   "{\"id\":10,\"fs_slug\":\"x\",\"rom_count\":2,\"name\":\"CON\"}]";

int main(void)
{
   static seen_t s;
   unsigned offset, id, n;
   bool multi;
   char system[64], slug[64], cover[128], name[128];

   /* platforms */
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(platforms, sizeof(platforms) - 1, "", collect, &s));
   CHECK(s.n == 2);
   CHECK(!strcmp(s.path[0], "0|2|Nintendo - Game Boy Advance|gba"));
   CHECK(!strcmp(s.label[0], "Game Boy Advance (120)"));
   CHECK(!strcmp(s.path[1], "0|7|My Homebrew|homebrew"));
   CHECK(!strcmp(s.label[1], "My Homebrew (3)"));

   /* ROMs: hostile names skipped, multi-file ones flagged, cover query
    * stripped,
    * a cover that is not under /assets dropped */
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(roms_page1, sizeof(roms_page1) - 1,
         "0|2|Nintendo - Game Boy Advance|gba", collect, &s));
   CHECK(s.n == 4);
   CHECK(!strcmp(s.path[0], "3|0|/assets/romm/resources/roms/1/3/cover/big.png|Dummy 001.gba"));
   CHECK(!strcmp(s.path[1], "4|1||Disc.cue"));
   CHECK(!strcmp(s.path[2], "6|0||Elsewhere.gba"));
   CHECK(s.pages == 1);
   CHECK(!strcmp(s.path[3], "100|2|Nintendo - Game Boy Advance|gba"));

   /* the last page has no next page */
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(roms_last, sizeof(roms_last) - 1,
         "100|2|Nintendo - Game Boy Advance|gba", collect, &s));
   CHECK(s.n == 1 && s.pages == 0);

   /* a total that exactly fills the page does not make one either */
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(roms_full, sizeof(roms_full) - 1,
         "0|2|S|gba", collect, &s));
   CHECK(s.n == 0);

   /* malformed, empty, wrong shape */
   memset(&s, 0, sizeof(s));
   CHECK(!romm_list_parse("[{\"id\":1", 8, "", collect, &s));
   CHECK(!romm_list_parse("", 0, "", collect, &s));
   CHECK(romm_list_parse(unauthorized, sizeof(unauthorized) - 1, "", collect, &s));
   CHECK(s.n == 0);

   /* the callback can stop the walk */
   n = 0;
   CHECK(!romm_list_parse(platforms, sizeof(platforms) - 1, "", stop_after_one, &n));

   /* list contexts */
   CHECK(romm_list_ctx_split("100|2|Nintendo - Game Boy Advance|gba",
         &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(offset == 100 && id == 2 && !strcmp(system, "Nintendo - Game Boy Advance")
         && !strcmp(slug, "gba"));
   CHECK(!romm_list_ctx_split("0|2|../x|gba", &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(!romm_list_ctx_split("0|0|S|gba", &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(!romm_list_ctx_split("0|2|S", &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(!romm_list_ctx_split("0|2|S|a\\b", &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(!romm_list_ctx_split("0|2|S|", &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   CHECK(!romm_list_ctx_split(NULL, &offset, &id, system, sizeof(system), slug, sizeof(slug)));
   /* the list fetch only wants the offset and the platform */
   CHECK(romm_list_ctx_split("200|7|S|gba", &offset, &id, NULL, 0, NULL, 0));
   CHECK(offset == 200 && id == 7);
   CHECK(!romm_list_ctx_split("200|0|S|gba", &offset, &id, NULL, 0, NULL, 0));

   /* ROM contexts */
   CHECK(romm_rom_ctx_split("3|0|/assets/a/b.png|Dummy 001.gba", &id, &multi,
         cover, sizeof(cover), name, sizeof(name)));
   CHECK(id == 3 && !strcmp(cover, "/assets/a/b.png") && !strcmp(name, "Dummy 001.gba"));
   CHECK(romm_rom_ctx_split("3|0||x.gba", &id, &multi, cover, sizeof(cover), name, sizeof(name)));
   CHECK(!romm_rom_ctx_split("3|0|/other/b.png|x.gba", &id, &multi, cover, sizeof(cover), name, sizeof(name)));
   CHECK(!romm_rom_ctx_split("3|0|/assets/../b.png|x.gba", &id, &multi, cover, sizeof(cover), name, sizeof(name)));
   CHECK(!romm_rom_ctx_split("3|0||..", &id, &multi, cover, sizeof(cover), name, sizeof(name)));
   CHECK(!romm_rom_ctx_split("3|0||a/b.gba", &id, &multi, cover, sizeof(cover), name, sizeof(name)));
   CHECK(!romm_rom_ctx_split("x|0||a.gba", &id, &multi, cover, sizeof(cover), name, sizeof(name)));

   CHECK(romm_name_is_safe("Nintendo - Game Boy Advance"));
   CHECK(romm_name_is_safe("Tom & Jerry (USA).gba"));
   CHECK(!romm_name_is_safe(""));
   CHECK(!romm_name_is_safe("."));
   CHECK(!romm_name_is_safe("a:b"));
   CHECK(!romm_name_is_safe("a\nb"));

   /* Windows device names and names it would rewrite */
   CHECK(!romm_name_is_safe("CON"));
   CHECK(!romm_name_is_safe("nul.gba"));
   CHECK(!romm_name_is_safe("Com1.gba"));
   CHECK(!romm_name_is_safe("LPT9"));
   CHECK(!romm_name_is_safe("aux.tar.gz"));
   CHECK(!romm_name_is_safe("con .txt"));
   CHECK(!romm_name_is_safe("CONIN$"));
   CHECK(!romm_name_is_safe("conout$.txt"));
   CHECK(!romm_name_is_safe("a."));
   CHECK(!romm_name_is_safe("a "));
   CHECK(!romm_name_is_safe("..."));
   CHECK(romm_name_is_safe("console.gba"));
   CHECK(romm_name_is_safe("com10.gba"));
   CHECK(romm_name_is_safe("com0"));
   CHECK(romm_name_is_safe(".hidden"));

   /* a cover cannot carry the field separator; a platform name is made
    * fit for a file; a NUL in a name drops its ROM; numbers are clamped */
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(odd_roms, sizeof(odd_roms) - 1, "4294967290|2|S|gba", collect, &s));
   CHECK(s.n == 1 && !strcmp(s.path[0], "5|0||pipe.gba") && s.pages == 0);
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(noname_platform, sizeof(noname_platform) - 1, "", collect, &s));
   CHECK(s.n == 1 && !strcmp(s.path[0], "0|11|Sys - X|fsx") && !strcmp(s.label[0], "fsx (3)"));
   memset(&s, 0, sizeof(s));
   CHECK(romm_list_parse(odd_platforms, sizeof(odd_platforms) - 1, "", collect, &s));
   CHECK(s.n == 1 && !strcmp(s.path[0], "0|9|Tom_Jerry_ - Remix|slug"));

   /* archive members: the zip-slip shapes are all refused */
   CHECK(romm_entry_is_safe("disc.cue"));
   CHECK(romm_entry_is_safe("Disc 1/track (01).bin"));
   CHECK(romm_entry_is_safe("Disc 1/"));
   CHECK(!romm_entry_is_safe(""));
   CHECK(!romm_entry_is_safe("/etc/passwd"));
   CHECK(!romm_entry_is_safe("../x.bin"));
   CHECK(!romm_entry_is_safe("a/../../x.bin"));
   CHECK(!romm_entry_is_safe("a/.."));
   CHECK(!romm_entry_is_safe("./a.bin"));
   CHECK(!romm_entry_is_safe("a//b.bin"));
   CHECK(!romm_entry_is_safe("C:/x.bin"));
   CHECK(!romm_entry_is_safe("C:x.bin"));
   CHECK(!romm_entry_is_safe("..\\x.bin"));
   CHECK(!romm_entry_is_safe("a\\b.bin"));
   CHECK(!romm_entry_is_safe("a/b.bin:stream"));
   CHECK(!romm_entry_is_safe("nul/x.bin"));
   CHECK(romm_entry_is_safe("a/b/c/d/e/f/g/h.bin"));
   CHECK(!romm_entry_is_safe("a/b/c/d/e/f/g/h/i.bin"));
   CHECK(!romm_entry_is_safe("a/b/c/d/e/f/g/h/i/"));
   CHECK(!romm_entry_is_safe("dir./x.bin"));
   CHECK(!romm_entry_is_safe("a/b\nc.bin"));

   /* what the playlist gets */
   {
      static const char *two[] = {
         "Game (Disc 2).cue", "Game (Disc 2).bin",
         "Game (Disc 1).cue", "Game (Disc 1).bin", "Game.m3u" };
      static const char *gdi[] = {
         "d.gdi", "track01.bin", "track02.raw", "track03.bin" };
      static const char *chds[] = {
         "G (Disc 10).chd", "G (Disc 2).chd", "G (Disc 1).chd", "G extra.chd" };
      static const char *tags[] = {
         "G (Disk 2 of 3).iso", "G (CD1).iso", "G (Disc C).iso" };
      static const char *bins[] = { "a.bin", "b.bin" };
      static const char *nes[] = { "rom.nes", "readme.txt", "sub/" };
      static const char *sub[] = { "Disc 2/g.cue", "Disc 2/g.bin",
         "Disc 1/g.cue", "Disc 1/g.bin" };
      static const char *only_m3u[] = { "a.m3u", "dir/" };
      static const char *dirs[] = { "G (Disc 10)/g.cue", "G (Disc 2)/g.cue",
         "G (Disc 1)/g.cue" };
      static const char *huge[] = {
         "G (Disc 99999999999999999999).cue", "G (Disc 2).cue" };
      static const uint32_t sz[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
      static const uint32_t nes_sz[3] = { 40000, 100, 0 };
      char target[64];
      char *m3u = NULL;

      CHECK(romm_archive_target(two, sz, 5, "Game.m3u", target, sizeof(target), &m3u));
      CHECK(!strcmp(target, "Game.m3u"));
      CHECK(m3u && !strcmp(m3u, "Game (Disc 1).cue\nGame (Disc 2).cue\n"));
      free(m3u);

      /* one disc: its descriptor, no playlist */
      CHECK(romm_archive_target(two + 2, sz, 2, "x.m3u", target, sizeof(target), &m3u));
      CHECK(!strcmp(target, "Game (Disc 1).cue") && !m3u);
      CHECK(romm_archive_target(gdi, sz, 4, "x.m3u", target, sizeof(target), &m3u));
      CHECK(!strcmp(target, "d.gdi") && !m3u);

      /* numeric disc order, untagged last */
      CHECK(romm_archive_target(chds, sz, 4, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strcmp(m3u,
            "G (Disc 1).chd\nG (Disc 2).chd\nG (Disc 10).chd\nG extra.chd\n"));
      free(m3u);
      CHECK(romm_archive_target(tags, sz, 3, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strcmp(m3u, "G (CD1).iso\nG (Disk 2 of 3).iso\nG (Disc C).iso\n"));
      free(m3u);

      /* raw images only count as discs when no descriptor claims them */
      CHECK(romm_archive_target(bins, sz, 2, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strcmp(m3u, "a.bin\nb.bin\n"));
      free(m3u);

      /* the discs keep their folder */
      CHECK(romm_archive_target(sub, sz, 4, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strcmp(m3u, "Disc 1/g.cue\nDisc 2/g.cue\n"));
      free(m3u);

      /* the number may be in the folder name, and may be absurd */
      CHECK(romm_archive_target(dirs, sz, 3, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strcmp(m3u, "G (Disc 1)/g.cue\nG (Disc 2)/g.cue\nG (Disc 10)/g.cue\n"));
      free(m3u);
      CHECK(romm_archive_target(huge, sz, 2, "G.m3u", target, sizeof(target), &m3u));
      CHECK(m3u && !strncmp(m3u, "G (Disc 2).cue\n", 15));
      free(m3u);

      /* not a disc set: the largest member */
      CHECK(romm_archive_target(nes, nes_sz, 3, "x.m3u", target, sizeof(target), &m3u));
      CHECK(!strcmp(target, "rom.nes") && !m3u);

      /* nothing to play, or no room for the answer */
      CHECK(!romm_archive_target(only_m3u, sz, 2, "x.m3u", target, sizeof(target), &m3u));
      CHECK(!romm_archive_target(two, sz, 0, "x.m3u", target, sizeof(target), &m3u));
      CHECK(!romm_archive_target(two, sz, 5, "Game.m3u", target, 4, &m3u) && !m3u);
   }

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("romm_list_test: all passed\n");
   return 0;
}

/* Regression test for c_converter merging DATs by a hex match key.
 *
 * A DAT may write a CRC as "abbe02fa" or "ABBE02FA".  Merged by
 * rom.crc, the two must give one record carrying the fields of both,
 * not two records that each hold half the game.
 *
 * c_converter.c is built with -Dmain=c_converter_main so its whole
 * path, lexer to written .rdb, runs here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../libretrodb.h"

int c_converter_main(int argc, char **argv);

#define DAT_LOWER "c_converter_case_lower.dat"
#define DAT_UPPER "c_converter_case_upper.dat"
#define RDB_OUT   "c_converter_case.rdb"

static int failures = 0;

static int write_text(const char *path, const char *text)
{
   FILE *f = fopen(path, "w");
   if (!f)
      return -1;
   fputs(text, f);
   fclose(f);
   return 0;
}

static const char *map_string(const struct rmsgpack_dom_value *item,
      const char *key)
{
   unsigned i;
   for (i = 0; i < item->val.map.len; i++)
   {
      const struct rmsgpack_dom_value *k = &item->val.map.items[i].key;
      const struct rmsgpack_dom_value *v = &item->val.map.items[i].value;
      if (   k->type == RDT_STRING
          && !strcmp(k->val.string.buff, key)
          && v->type == RDT_STRING)
         return v->val.string.buff;
   }
   return NULL;
}

static void check(int ok, const char *what)
{
   printf("[%s] %s\n", ok ? "pass" : "FAIL", what);
   if (!ok)
      failures++;
}

int main(void)
{
   char *argv[5];
   libretrodb_t        *db  = libretrodb_new();
   libretrodb_cursor_t *cur = libretrodb_cursor_new();
   struct rmsgpack_dom_value item;
   long records             = 0;
   int named_with_genre     = 0;

   if (   write_text(DAT_LOWER,
            "game (\n\tname \"Game\"\n\trom ( name \"Game.nes\" crc abbe02fa )\n)\n")
       || write_text(DAT_UPPER,
            "game (\n\tcomment \"Game\"\n\tgenre \"Action\"\n\trom ( crc ABBE02FA )\n)\n"))
   {
      printf("[FAIL] could not write the DAT files\n");
      return 1;
   }

   argv[0] = (char*)"c_converter";
   argv[1] = (char*)RDB_OUT;
   argv[2] = (char*)"rom.crc";
   argv[3] = (char*)DAT_LOWER;
   argv[4] = (char*)DAT_UPPER;
   check(c_converter_main(5, argv) == 0, "c_converter wrote the database");

   if (   !db || !cur
       || libretrodb_open(RDB_OUT, db, false) != 0
       || libretrodb_cursor_open(db, cur, NULL) != 0)
   {
      printf("[FAIL] could not read %s\n", RDB_OUT);
      return 1;
   }

   while (libretrodb_cursor_read_item(cur, &item) == 0)
   {
      if (item.type == RDT_MAP)
      {
         const char *name  = map_string(&item, "name");
         const char *genre = map_string(&item, "genre");
         records++;
         if (   name  && !strcmp(name,  "Game")
             && genre && !strcmp(genre, "Action"))
            named_with_genre = 1;
      }
      rmsgpack_dom_value_free(&item);
   }
   libretrodb_cursor_close(cur);
   libretrodb_cursor_free(cur);
   libretrodb_close(db);
   libretrodb_free(db);

   printf("records: %ld\n", records);
   check(records == 1, "\"abbe02fa\" and \"ABBE02FA\" merge into one record");
   check(named_with_genre, "the record has the name of one DAT and the genre of the other");

   remove(DAT_LOWER);
   remove(DAT_UPPER);
   remove(RDB_OUT);

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}

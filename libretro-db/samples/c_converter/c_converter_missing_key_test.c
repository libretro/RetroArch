/* Regression test for c_converter skipping an entry without the
 * match key.
 *
 * One game with no rom.crc is reported and left out; the games after
 * it in the same DAT are still written. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../libretrodb.h"

int c_converter_main(int argc, char **argv);

#define DAT_IN  "c_converter_missing_key.dat"
#define RDB_OUT "c_converter_missing_key.rdb"

static int failures = 0;

static void check(int ok, const char *what)
{
   printf("[%s] %s\n", ok ? "pass" : "FAIL", what);
   if (!ok)
      failures++;
}

int main(void)
{
   char *argv[4];
   FILE *f;
   libretrodb_t        *db  = libretrodb_new();
   libretrodb_cursor_t *cur = libretrodb_cursor_new();
   struct rmsgpack_dom_value item;
   long records             = 0;

   if (!(f = fopen(DAT_IN, "w")))
   {
      printf("[FAIL] could not write the DAT file\n");
      return 1;
   }
   fputs("game (\n\tname \"No ROM\"\n)\n"
         "game (\n\tname \"A\"\n\trom ( crc 11111111 )\n)\n"
         "game (\n\tname \"B\"\n\trom ( crc 22222222 )\n)\n", f);
   fclose(f);

   argv[0] = (char*)"c_converter";
   argv[1] = (char*)RDB_OUT;
   argv[2] = (char*)"rom.crc";
   argv[3] = (char*)DAT_IN;
   check(c_converter_main(4, argv) == 0, "c_converter wrote the database");

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
         records++;
      rmsgpack_dom_value_free(&item);
   }
   libretrodb_cursor_close(cur);
   libretrodb_cursor_free(cur);
   libretrodb_close(db);
   libretrodb_free(db);

   printf("records: %ld\n", records);
   check(records == 2, "the two games after the one without a CRC are written");

   remove(DAT_IN);
   remove(RDB_OUT);

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}

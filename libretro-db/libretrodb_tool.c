#include <stdio.h>
#include <string.h>

#include "libretrodb.h"
#include "rmsgpack_dom.h"

int main(int argc, char ** argv)
{
   int rv;
   libretrodb_t db;
   libretrodb_cursor_t cur;
   libretrodb_query_t *q;
   struct rmsgpack_dom_value item;
   const char *command, *path, *query_exp, *error;

   if (argc < 3)
   {
      printf("Usage: %s <db file> <command> [extra args...]\n", argv[0]);
      printf("Available Commands:\n");
      printf("\tlist\n");
      printf("\tcreate-index <index name> <field name>\n");
      printf("\tfind <query expression>\n");
      return 1;
   }

   command = argv[2];
   path    = argv[1];

   if ((rv = libretrodb_open(path, &db)) != 0)
   {
      printf("Could not open db file '%s': %s\n", path, strerror(-rv));
      return 1;
   }
   else if (!strcmp(command, "list"))
   {
      if ((rv = libretrodb_cursor_open(&db, &cur, NULL)) != 0)
      {
         printf("Could not open cursor: %s\n", strerror(-rv));
         return 1;
      }

      if (argc != 3)
      {
         printf("Usage: %s <db file> list\n", argv[0]);
         return 1;
      }

      while (libretrodb_cursor_read_item(&cur, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else if (!strcmp(command, "find"))
   {
      if (argc != 4)
      {
         printf("Usage: %s <db file> find <query expression>\n", argv[0]);
         return 1;
      }

      query_exp = argv[3];
      error = NULL;
      q = libretrodb_query_compile(&db, query_exp, strlen(query_exp), &error);

      if (error)
      {
         printf("%s\n", error);
         return 1;
      }

      if ((rv = libretrodb_cursor_open(&db, &cur, q)) != 0)
      {
         printf("Could not open cursor: %s\n", strerror(-rv));
         return 1;
      }

      while (libretrodb_cursor_read_item(&cur, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else if (!strcmp(command, "create-index"))
   {
      const char * index_name, * field_name;

      if (argc != 5)
      {
         printf("Usage: %s <db file> create-index <index name> <field name>\n", argv[0]);
         return 1;
      }

      index_name = argv[3];
      field_name = argv[4];

      libretrodb_create_index(&db, index_name, field_name);
   }
   else
   {
      printf("Unknown command %s\n", argv[2]);
      return 1;
   }

   libretrodb_close(&db);
   return 1;
}

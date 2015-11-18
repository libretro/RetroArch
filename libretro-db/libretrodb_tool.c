#include <stdio.h>
#include <string.h>

#include "libretrodb.h"
#include "rmsgpack_dom.h"

int main(int argc, char ** argv)
{
   int rv;
   libretrodb_t *db;
   libretrodb_cursor_t *cur;
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

   db  = libretrodb_new();
   cur = libretrodb_cursor_new();

   if (!db || !cur)
      goto error;

   if ((rv = libretrodb_open(path, db)) != 0)
   {
      printf("Could not open db file '%s': %s\n", path, strerror(-rv));
      goto error;
   }
   else if (strcmp(command, "list") == 0)
   {
      if ((rv = libretrodb_cursor_open(db, cur, NULL)) != 0)
      {
         printf("Could not open cursor: %s\n", strerror(-rv));
         goto error;
      }

      if (argc != 3)
      {
         printf("Usage: %s <db file> list\n", argv[0]);
         goto error;
      }

      while (libretrodb_cursor_read_item(cur, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else if (strcmp(command, "find") == 0)
   {
      if (argc != 4)
      {
         printf("Usage: %s <db file> find <query expression>\n", argv[0]);
         goto error;
      }

      query_exp = argv[3];
      error = NULL;
      q = libretrodb_query_compile(db, query_exp, strlen(query_exp), &error);

      if (error)
      {
         printf("%s\n", error);
         goto error;
      }

      if ((rv = libretrodb_cursor_open(db, cur, q)) != 0)
      {
         printf("Could not open cursor: %s\n", strerror(-rv));
         goto error;
      }

      while (libretrodb_cursor_read_item(cur, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else if (strcmp(command, "create-index") == 0)
   {
      const char * index_name, * field_name;

      if (argc != 5)
      {
         printf("Usage: %s <db file> create-index <index name> <field name>\n", argv[0]);
         goto error;
      }

      index_name = argv[3];
      field_name = argv[4];

      libretrodb_create_index(db, index_name, field_name);
   }
   else
   {
      printf("Unknown command %s\n", argv[2]);
      goto error;
   }

   libretrodb_close(db);

error:
   if (db)
      libretrodb_free(db);
   if (cur)
      libretrodb_cursor_free(cur);
   return 1;
}

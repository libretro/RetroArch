#include <stdio.h>
#include <string.h>

#include "rarchdb.h"
#include "rmsgpack_dom.h"

static int list_db(void)
{
	return 0;
}

int main(int argc, char** argv)
{
   int rv;
   struct rarchdb db;
   struct rmsgpack_dom_value item;
   if (argc < 3)
   {
      printf("Usage: %s <db file> <command> [extra args...]\n", argv[0]);
      printf("Available Commands:\n");
      printf("\tlist\n");
      printf("\tcreate-index <index name> <field name>\n");
      printf("\tfind <index name> <value>\n");
      return 1;
   }

   const char* command = argv[2];
   const char* path = argv[1];

   if ((rv = rarchdb_open(path, &db)) != 0)
   {
      printf("Could not open db file '%s': %s\n", path, strerror(-rv));
      return 1;
   }

   if (strcmp(command, "list") == 0)
   {
      if (argc != 3)
      {
         printf("Usage: %s <db file> list\n", argv[0]);
         return 1;
      }
      while(rarchdb_read_item(&db, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else if (strcmp(command, "create-index") == 0)
   {
      const char *index_name, *field_name;

      if (argc != 5)
      {
         printf("Usage: %s <db file> create-index <index name> <field name>\n", argv[0]);
         return 1;
      }

      index_name = argv[3];
      field_name = argv[4];

      rarchdb_create_index(&db, index_name, field_name);
   }
   else if (strcmp(command, "find") == 0)
   {
      int i;
      const char *index_name, *value;
      char bin_value[256];
      uint8_t h;
      uint8_t l;

      if (argc != 5)
      {
         printf("Usage: %s <db file> create-index <index name> <field name>\n", argv[0]);
         return 1;
      }

      index_name = argv[3];
      value = argv[4];

      for (i = 0; i < strlen(value); i += 2)
      {
         if (value[i] <= '9')
            h = value[i] - '0';
         else
            h = (value[i] - 'A') + 10;

         if (value[i+1] <= '9')
            l = value[i+1] - '0';
         else
            l = (value[i+1] - 'A') + 10;

         bin_value[i/2] = h * 16 + l;
      }

      if (rarchdb_find_entry(&db, index_name, bin_value) != 0)
      {
         printf("Could not find item\n");
         return 1;
      }

      if (rarchdb_read_item(&db, &item) == 0)
      {
         rmsgpack_dom_value_print(&item);
         printf("\n");
         rmsgpack_dom_value_free(&item);
      }
   }
   else
   {
      printf("Unkown command %s\n", argv[1]);
      return 1;
   }
   rarchdb_close(&db);
}

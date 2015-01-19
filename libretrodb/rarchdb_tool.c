#include <stdio.h>
#include <string.h>

#include "rarchdb.h"
#include "rmsgpack_dom.h"

int main(
        int argc,
        char ** argv
){
	int rv;
	struct rarchdb db;
	struct rarchdb_cursor cur;
	struct rmsgpack_dom_value item;
	if (argc < 3) {
		printf("Usage: %s <db file> <command> [extra args...]\n", argv[0]);
		printf("Available Commands:\n");
		printf("\tlist\n");
		printf("\tcreate-index <index name> <field name>\n");
		printf("\tfind <query expression>\n");
		return 1;
	}

	const char * command = argv[2];
	const char * path = argv[1];

	if ((rv = rarchdb_open(path, &db)) != 0) {
		printf("Could not open db file '%s': %s\n", path, strerror(-rv));
		return 1;
	}
	else if (strcmp(command, "list") == 0) {
		if ((rv = rarchdb_cursor_open(&db, &cur, NULL)) != 0) {
			printf("Could not open cursor: %s\n", strerror(-rv));
			return 1;
		}

		if (argc != 3) {
			printf("Usage: %s <db file> list\n", argv[0]);
			return 1;
		}
		while (rarchdb_cursor_read_item(&cur, &item) == 0) {
			rmsgpack_dom_value_print(&item);
			printf("\n");
			rmsgpack_dom_value_free(&item);
		}
	}
	else if (strcmp(command, "find") == 0) {
		if (argc != 4) {
			printf("Usage: %s <db file> find <query expression>\n", argv[0]);
			return 1;
		}
		const char * query_exp = argv[3];
		const char * error = NULL;
		rarchdb_query * q = rarchdb_query_compile(&db, query_exp, strlen(query_exp), &error);
		if (error) {
			printf("%s\n", error);
			return 1;
		}

		if ((rv = rarchdb_cursor_open(&db, &cur, q)) != 0) {
			printf("Could not open cursor: %s\n", strerror(-rv));
			return 1;
		}

		while (rarchdb_cursor_read_item(&cur, &item) == 0) {
			rmsgpack_dom_value_print(&item);
			printf("\n");
			rmsgpack_dom_value_free(&item);
		}
	} else if (strcmp(command, "create-index") == 0) {
		const char * index_name, * field_name;

		if (argc != 5) {
			printf("Usage: %s <db file> create-index <index name> <field name>\n", argv[0]);
			return 1;
		}

		index_name = argv[3];
		field_name = argv[4];

		rarchdb_create_index(&db, index_name, field_name);
	} else {
		printf("Unkown command %s\n", argv[1]);
		return 1;
	}
	rarchdb_close(&db);
}

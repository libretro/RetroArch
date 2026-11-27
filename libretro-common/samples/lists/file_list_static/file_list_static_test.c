/* Entry strings inside a registered read-only table are lent to the
 * list and never freed; everything else is still copied and freed.
 * Run under ASan: a free() of a lent string is reported as a bad free,
 * and a lent string that is copied anyway leaks nothing but fails the
 * pointer checks below. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lists/file_list.h>

static const char table[] = "playlist_entry\0explore_item\0main_menu";
#define T_PLAYLIST  (table)
#define T_EXPLORE   (table + 15)
#define T_MAIN      (table + 28)

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

int main(void)
{
   file_list_t list;
   char outside[]   = "playlist_entry";
   char straddle[2] = { 'x', '\0' };
   size_t i;

   memset(&list, 0, sizeof(list));

   /* Unregistered: copied */
   CHECK(file_list_append(&list, T_PLAYLIST, T_EXPLORE, 0, 0, 0));
   CHECK(list.list[0].path  != T_PLAYLIST);
   CHECK(list.list[0].label != T_EXPLORE);
   CHECK(!strcmp(list.list[0].label, "explore_item"));

   CHECK(!file_list_add_static_strings(NULL, 4));
   CHECK(!file_list_add_static_strings(table, 0));
   /* Leave the last string outside the range: a pointer at its end
    * is not inside it */
   CHECK(file_list_add_static_strings(table, (size_t)(T_MAIN - table)));
   CHECK(file_list_add_static_strings(table, (size_t)(T_MAIN - table)));

   /* Registered: lent, and the earlier copies still free normally */
   CHECK(file_list_append(&list, T_PLAYLIST, T_EXPLORE, 0, 0, 0));
   CHECK(list.list[1].path  == T_PLAYLIST);
   CHECK(list.list[1].label == T_EXPLORE);

   /* Same bytes elsewhere, a string past the range, empty: as before */
   CHECK(file_list_append(&list, outside, T_MAIN, 0, 0, 0));
   CHECK(list.list[2].path  != outside);
   CHECK(list.list[2].label != T_MAIN);
   CHECK(!strcmp(list.list[2].label, "main_menu"));
   CHECK(file_list_append(&list, "", straddle, 0, 0, 0));
   CHECK(list.list[3].label != straddle);

   /* Insert lends too */
   CHECK(file_list_insert(&list, T_EXPLORE, T_PLAYLIST, 0, 0, 0, 0));
   CHECK(list.list[0].path  == T_EXPLORE);
   CHECK(list.list[0].label == T_PLAYLIST);

   /* Relabel lent -> owned -> lent; alt the same way */
   file_list_set_label_at_offset(&list, 2, "owned label");
   CHECK(!strcmp(list.list[2].label, "owned label"));
   file_list_set_label_at_offset(&list, 2, T_PLAYLIST);
   CHECK(list.list[2].label == T_PLAYLIST);
   file_list_set_label_at_offset(&list, 2, T_EXPLORE);
   CHECK(list.list[2].label == T_EXPLORE);
   file_list_set_alt_at_offset(&list, 1, T_PLAYLIST);
   CHECK(list.list[1].alt == T_PLAYLIST);
   file_list_set_alt_at_offset(&list, 1, "owned alt");
   CHECK(!strcmp(list.list[1].alt, "owned alt"));
   file_list_free_label(&list, 1);
   CHECK(list.list[1].label == NULL);

   /* Sorting moves lent entries with the rest */
   file_list_sort_on_alt(&list);
   for (i = 0; i < list.size; i++)
      CHECK(list.list[i].path != NULL);

   /* Pop and teardown release only what was copied */
   file_list_pop(&list, NULL);
   file_list_deinitialize(&list);

   /* The registry is bounded; a full one copies */
   {
      static const char a[] = "a", b[] = "b", c[] = "c", d[] = "d";
      file_list_add_static_strings(a, sizeof(a));
      file_list_add_static_strings(b, sizeof(b));
      file_list_add_static_strings(c, sizeof(c));
      CHECK(!file_list_add_static_strings(d, sizeof(d)));
      memset(&list, 0, sizeof(list));
      CHECK(file_list_append(&list, a, d, 0, 0, 0));
      CHECK(list.list[0].path  == a);
      CHECK(list.list[0].label != d);
      file_list_deinitialize(&list);
   }

   if (failures)
   {
      fprintf(stderr, "file_list_static: %d failure(s)\n", failures);
      return 1;
   }
   printf("file_list_static: all checks passed\n");
   return 0;
}

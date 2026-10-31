/* Allocation failures must not publish entries or move existing ones. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned calls, fail_at, live;

static char *test_strdup(const char *s)
{
   size_t len;
   char *p;
   if (++calls == fail_at)
      return NULL;
   len = strlen(s) + 1;
   p = (char*)malloc(len);
   assert(p);
   memcpy(p, s, len);
   live++;
   return p;
}

static void *test_realloc(void *p, size_t size)
{
   void *result;
   if (++calls == fail_at)
      return NULL;
   result = realloc(p, size);
   assert(result);
   if (!p)
      live++;
   return result;
}

static void test_free(void *p)
{
   if (p)
   {
      assert(live);
      live--;
   }
   free(p);
}

#define strdup test_strdup
#define realloc test_realloc
#define free test_free
#include "../../../lists/file_list.c"
#undef strdup
#undef realloc
#undef free

int main(void)
{
   unsigned fault, insert;
   for (insert = 0; insert < 2; insert++)
      for (fault = 1; fault <= 3; fault++)
      {
         file_list_t list;
         memset(&list, 0, sizeof(list));
         fail_at = 0;
         assert(file_list_append(&list, "original", "label", 0, 0, 0));
         calls = 0;
         fail_at = fault;
         if (insert)
            assert(!file_list_insert(&list, "new", "new label", 0, 0, 0, 0));
         else
            assert(!file_list_append(&list, "new", "new label", 0, 0, 0));
         fail_at = 0;
         assert(list.size == 1);
         assert(!strcmp(list.list[0].path, "original"));
         assert(!strcmp(list.list[0].label, "label"));
         assert(file_list_insert(&list, "retry", "ok", 0, 0, 0, 0));
         assert(list.size == 2);
         assert(!strcmp(list.list[1].path, "original"));
         assert(!file_list_insert(&list, "invalid", "index", 0, 0, 0, list.size + 1));
         file_list_deinitialize_internal(&list);
         assert(!live);
      }
   {
      file_list_t list;
      memset(&list, 0, sizeof(list));
      calls = 0;
      assert(file_list_reserve(&list, 4));
      assert(calls == 1);
      assert(file_list_reserve(&list, 4));
      assert(calls == 1);
      assert(file_list_append(&list, "", "", 0, 0, 0));
      assert(file_list_append(&list, NULL, NULL, 0, 0, 0));
      assert(calls == 1);
      assert(list.list[0].path && !*list.list[0].path);
      assert(!list.list[1].path);
      file_list_deinitialize_internal(&list);
      assert(!live);
   }
   puts("PASS: append/insert allocation failures, rollback, retry, empty strings, reserve reuse");
   return 0;
}

/* A packed split matches string_split, is one block, and stays read-only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lists/string_list.h>

static int fails;

static void check(const char *what, int ok)
{
   if (!ok)
   {
      fails++;
      printf("FAIL  %s\n", what);
   }
}

static void same_as_split(const char *str, const char *delim)
{
   size_t i;
   char what[256];
   struct string_list *a = string_split(str, delim);
   struct string_list *b = string_split_packed(str, delim);

   snprintf(what, sizeof(what), "\"%s\" by \"%s\" splits as string_split",
         str, delim);
   if (!a || !b || a->size != b->size)
   {
      check(what, 0);
      string_list_free(a);
      string_list_free(b);
      return;
   }
   for (i = 0; i < a->size; i++)
      if (strcmp(a->elems[i].data, b->elems[i].data)
            || b->elems[i].userdata || b->elems[i].attr.i)
         check(what, 0);

   /* Every element and token inside the one block */
   snprintf(what, sizeof(what), "\"%s\" is one block", str);
   for (i = 0; i < b->size; i++)
      if (     (char*)&b->elems[i] < (char*)(b + 1)
            || b->elems[i].data < (char*)(b->elems + b->size)
            || b->elems[i].data + strlen(b->elems[i].data)
               > (char*)(b->elems + b->size) + strlen(str))
         check(what, 0);

   string_list_free(a);
   string_list_free(b);
}

int main(void)
{
   union string_list_elem_attr attr;
   struct string_list *packed;
   struct string_list *clone;

   same_as_split("", "|");
   same_as_split("|||", "|");
   same_as_split("cue", "|");
   same_as_split("cue|chd|iso", "|");
   same_as_split("|cue||chd|", "|");
   same_as_split("a b,c;;d", " ,;");
   same_as_split("Fran\xc3\xa7ois|\xe6\x97\xa5\xe6\x9c\xac", "|");

   packed = string_split_packed("cue|chd|iso", "|");
   check("packed split allocates", packed != NULL);
   if (!packed)
      return 1;
   check("packed list finds its tokens",
         string_list_find_elem(packed, "chd") == 2
         && !string_list_find_elem(packed, "bin"));

   /* Read-only: growing it would realloc into the middle of the block */
   attr.i = 0;
   check("append is refused", !string_list_append(packed, "bin", attr));
   check("append_n is refused",
         !string_list_append_n(packed, "bin", 3, attr));
   check("capacity is refused", !string_list_capacity(packed, 64));
   check("deinitialize is refused", !string_list_deinitialize(packed));
   check("refusals leave it whole", packed->size == 3
         && !strcmp(packed->elems[2].data, "iso"));

   /* A clone is an ordinary list again */
   clone = string_list_clone(packed);
   check("clone allocates", clone != NULL);
   if (clone)
   {
      check("clone appends", string_list_append(clone, "bin", attr)
            && clone->size == 4 && !strcmp(clone->elems[3].data, "bin"));
      string_list_free(clone);
   }
   string_list_free(packed);

   check("NULL input", string_split_packed(NULL, "|") == NULL);

   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("ok    string_split_packed: matches string_split in one read-only block\n");
   return 0;
}

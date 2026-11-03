/* Allocation failures must release owned input without reading a partial tree. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_count, failure_at, live_allocations, input_frees;
static void *owned_input;

static void *test_malloc(size_t size)
{
   void *p;
   if (++allocation_count == failure_at)
      return NULL;
   p = malloc(size);
   if (p)
   {
      ++live_allocations;
      memset(p, 0xa5, size);
   }
   return p;
}

static void *test_calloc(size_t count, size_t size)
{
   void *p;
   if (++allocation_count == failure_at)
      return NULL;
   p = calloc(count, size);
   if (p)
      ++live_allocations;
   return p;
}

static void *test_realloc(void *old, size_t size)
{
   void *p;
   int was_null = old == NULL;
   if (++allocation_count == failure_at)
      return NULL;
   p = realloc(old, size);
   if (p && was_null)
      ++live_allocations;
   return p;
}

static void test_free(void *p)
{
   if (p)
   {
      if (p == owned_input)
         ++input_frees;
      else
      {
         assert(live_allocations);
         --live_allocations;
      }
   }
   free(p);
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#include "../../formats/xml/rxml.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void exercise(size_t nth, int incremental)
{
   static const char xml[] =
      "<root a='x&amp;y'><child>one</child><child><![CDATA[two]]></child></root>";
   rxml_document_t *doc = NULL;
   rxml_parse_error_t error;
   rxml_parse_t *parse;
   int state;
   owned_input = malloc(sizeof(xml));
   assert(owned_input);
   memcpy(owned_input, xml, sizeof(xml));
   allocation_count = live_allocations = input_frees = 0;
   failure_at = nth;
   if (incremental)
   {
      parse = rxml_parse_begin_owned((char*)owned_input,
            sizeof(xml) - 1, RXML_OPT_STRICT_EOF);
      if (parse)
      {
         do { state = rxml_parse_step(parse, 3); } while (!state);
         doc = rxml_parse_end(parse);
      }
   }
   else
      doc = rxml_load_document_owned_opts((char*)owned_input,
            sizeof(xml) - 1, RXML_OPT_STRICT_EOF, &error);
   if (!nth)
   {
      assert(doc);
      assert(!strcmp(rxml_root_node(doc)->name, "root"));
   }
   rxml_free_document(doc);
   assert(!live_allocations);
   assert(input_frees == 1);
}

int main(void)
{
   size_t nth;
   for (nth = 0; nth <= 32; ++nth)
   {
      exercise(nth, 0);
      exercise(nth, 1);
   }
   puts("rxml owned allocation failures: PASS");
   return 0;
}

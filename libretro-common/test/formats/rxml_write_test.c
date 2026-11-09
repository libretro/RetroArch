/* rxml's writer: exact output, everything escaped parses back unchanged,
 * a parsed tree written back is the same tree, misuse and allocation
 * failure stop the writer without leaking. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_count, failure_at, live_allocations;

static void *test_malloc(size_t size)
{
   void *p;
   if (++allocation_count == failure_at)
      return NULL;
   p = malloc(size);
   if (p)
      ++live_allocations;
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
      assert(live_allocations);
      --live_allocations;
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

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static int same_tree(const struct rxml_node *a, const struct rxml_node *b)
{
   for (; a && b; a = a->next, b = b->next)
   {
      const struct rxml_attrib_node *x = a->attrib, *y = b->attrib;
      if (strcmp(a->name, b->name))
         return 0;
      if (!a->data != !b->data || (a->data && strcmp(a->data, b->data)))
         return 0;
      for (; x && y; x = x->next, y = y->next)
         if (strcmp(x->attrib, y->attrib) || !x->value != !y->value
               || (x->value && strcmp(x->value, y->value)))
            return 0;
      if (x || y || !same_tree(a->children, b->children))
         return 0;
   }
   return !a && !b;
}

static void test_layout(void)
{
   const char *out;
   size_t len = 0;
   rxml_writer_t *w = rxml_writer_new("\t");
   rxml_write_declaration(w);
   rxml_write_open(w, "title_list");
   rxml_write_open(w, "title");
   rxml_write_attrib(w, "titleId", "0005000010101c00");
   rxml_write_element(w, "name", "Wind Waker");
   rxml_write_element(w, "sub_path", NULL);
   rxml_write_close(w);
   rxml_write_open(w, "empty");
   rxml_write_close(w);
   rxml_write_close(w);
   out = rxml_writer_data(w, &len);
   CHECK(out && !strcmp(out,
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<title_list>\n"
         "\t<title titleId=\"0005000010101c00\">\n"
         "\t\t<name>Wind Waker</name>\n"
         "\t\t<sub_path />\n"
         "\t</title>\n"
         "\t<empty />\n"
         "</title_list>\n"));
   CHECK(out && len == strlen(out));
   rxml_writer_free(w);

   w = rxml_writer_new(NULL);
   rxml_write_open(w, "a");
   rxml_write_attrib(w, "x", "1");
   rxml_write_element(w, "b", "t");
   rxml_write_open(w, "c");
   rxml_write_close(w);
   rxml_write_close(w);
   out = rxml_writer_data(w, NULL);
   CHECK(out && !strcmp(out, "<a x=\"1\"><b>t</b><c /></a>"));
   rxml_writer_free(w);
}

static void test_escapes(void)
{
   /* every character that needs a reference, in text and attribute */
   static const char text[]   = "a & b < c > d \"q\" 'a' ]]> tab\there\nline\r\x01\x1f end";
   static const char attrib[] = "x & < > \"q\" 'a' tab\there\nline\r\x01 end";
   rxml_document_t *doc;
   struct rxml_node *root;
   const char *out;
   rxml_writer_t *w = rxml_writer_new("  ");
   rxml_write_open(w, "r");
   rxml_write_attrib(w, "v", attrib);
   rxml_write_text(w, text);
   rxml_write_close(w);
   out = rxml_writer_data(w, NULL);
   CHECK(out != NULL);
   CHECK(out && !strstr(out, "]]>"));
   doc  = out ? rxml_load_document_string(out) : NULL;
   root = doc ? rxml_root_node(doc) : NULL;
   CHECK(root != NULL);
   if (root)
   {
      CHECK(root->data && !strcmp(root->data, text));
      CHECK(rxml_node_attrib(root, "v")
            && !strcmp(rxml_node_attrib(root, "v"), attrib));
   }
   rxml_free_document(doc);
   rxml_writer_free(w);

   /* UTF-8 passes through as it is */
   w = rxml_writer_new(NULL);
   rxml_write_element(w, "n", "\xc3\xa9\xe3\x82\xbc\xe3\x83\xab\xe3\x83\x80");
   out = rxml_writer_data(w, NULL);
   CHECK(out && !strcmp(out, "<n>\xc3\xa9\xe3\x82\xbc\xe3\x83\xab\xe3\x83\x80</n>"));
   rxml_writer_free(w);
}

static void test_node_roundtrip(void)
{
   static const char xml[] =
      "<?xml version=\"1.0\"?>\n"
      "<saveinfo a=\"1\" b=\"&amp;&quot;\">\n"
      "  <account persistentId=\"80000001\"><timestamp>1234</timestamp></account>\n"
      "  <account persistentId=\"80000002\"/>\n"
      "  <deep><d1><d2><d3>x &lt; y</d3></d2></d1><after/></deep>\n"
      "  <last>tail</last>\n"
      "</saveinfo>\n";
   rxml_document_t *doc = rxml_load_document_string(xml), *doc2;
   const char *indents[2];
   int i;
   indents[0] = "\t";
   indents[1] = NULL;
   CHECK(doc != NULL);
   if (!doc)
      return;
   for (i = 0; i < 2; i++)
   {
      rxml_writer_t *w = rxml_writer_new(indents[i]);
      const char *out;
      rxml_write_declaration(w);
      rxml_write_node(w, rxml_root_node(doc));
      out  = rxml_writer_data(w, NULL);
      CHECK(out != NULL);
      doc2 = out ? rxml_load_document_string(out) : NULL;
      CHECK(doc2 && same_tree(rxml_root_node(doc), rxml_root_node(doc2)));
      rxml_free_document(doc2);
      rxml_writer_free(w);
   }

   /* a subtree, inside elements of the caller's, and only that subtree:
    * the node's own siblings are not written */
   {
      rxml_writer_t *w = rxml_writer_new(NULL);
      const char *out;
      rxml_write_open(w, "wrap");
      rxml_write_node(w, rxml_root_node(doc)->children);
      rxml_write_element(w, "added", "1");
      rxml_write_close(w);
      out = rxml_writer_data(w, NULL);
      CHECK(out && !strcmp(out, "<wrap><account persistentId=\"80000001\">"
               "<timestamp>1234</timestamp></account><added>1</added></wrap>"));
      rxml_writer_free(w);
   }
   rxml_free_document(doc);
}

static void test_deep(void)
{
   /* far deeper than any parsed document: the writer keeps its own
    * stack and grows it */
   enum { DEPTH = 100000 };
   rxml_writer_t *w = rxml_writer_new(NULL);
   const char *out;
   size_t len = 0, i;
   for (i = 0; i < DEPTH; i++)
      rxml_write_open(w, i & 1 ? "ab" : "c");
   rxml_write_text(w, "x");
   for (i = 0; i < DEPTH; i++)
      rxml_write_close(w);
   out = rxml_writer_data(w, &len);
   CHECK(out != NULL);
   CHECK(len == (size_t)DEPTH / 2 * (4 + 3 + 5 + 4) + 1);
   CHECK(out && !strncmp(out, "<c><ab><c>", 10));
   CHECK(out && !strcmp(out + len - 9, "</ab></c>"));
   rxml_writer_free(w);
}

static void test_misuse(void)
{
   rxml_writer_t *w;

#define MISUSE(calls) \
   w = rxml_writer_new(NULL); \
   calls; \
   CHECK(rxml_writer_failed(w)); \
   CHECK(rxml_writer_data(w, NULL) == NULL); \
   rxml_writer_free(w)

   MISUSE(rxml_write_open(w, "a"); rxml_write_close(w); rxml_write_open(w, "b"));
   MISUSE(rxml_write_open(w, "a"); rxml_write_text(w, "t"); rxml_write_attrib(w, "x", "1"));
   MISUSE(rxml_write_open(w, "a"); rxml_write_element(w, "b", NULL); rxml_write_attrib(w, "x", "1"));
   MISUSE(rxml_write_text(w, "t"));
   MISUSE(rxml_write_close(w));
   MISUSE(rxml_write_open(w, ""));
   MISUSE(rxml_write_open(w, NULL));
   MISUSE(rxml_write_open(w, "a b"));
   MISUSE(rxml_write_open(w, "a>"));
   MISUSE(rxml_write_open(w, "a"); rxml_write_attrib(w, "x\"", "1"));
   MISUSE(rxml_write_open(w, "a"); rxml_write_attrib(w, "x", NULL));
   MISUSE(rxml_write_open(w, "a"); rxml_write_close(w); rxml_write_declaration(w));
   MISUSE(rxml_write_node(w, NULL));
#undef MISUSE

   /* not failed, but not a document yet */
   w = rxml_writer_new(NULL);
   CHECK(rxml_writer_data(w, NULL) == NULL);
   rxml_write_declaration(w);
   rxml_write_open(w, "a");
   CHECK(!rxml_writer_failed(w));
   CHECK(rxml_writer_data(w, NULL) == NULL);
   rxml_write_close(w);
   CHECK(rxml_writer_data(w, NULL) != NULL);
   rxml_writer_free(w);
   rxml_writer_free(NULL);
}

/* Every allocation in turn fails: the writer stops (or the constructor
 * returns NULL), nothing leaks */
static void test_oom(void)
{
   size_t nth;
   for (nth = 1; nth < 64; nth++)
   {
      rxml_writer_t *w;
      size_t i;
      allocation_count = 0;
      failure_at       = nth;
      w = rxml_writer_new("\t");
      if (w)
      {
         rxml_write_declaration(w);
         rxml_write_open(w, "root");
         for (i = 0; i < 40; i++)
         {
            rxml_write_open(w, "item");
            rxml_write_attrib(w, "k", "v & w");
            rxml_write_text(w, "some text to make the buffer grow");
            rxml_write_close(w);
         }
         rxml_write_close(w);
         CHECK(!rxml_writer_data(w, NULL) == !!rxml_writer_failed(w));
         rxml_writer_free(w);
      }
      CHECK(live_allocations == 0);
   }
   failure_at = 0;
}

int main(void)
{
   test_layout();
   test_escapes();
   test_node_roundtrip();
   test_deep();
   test_misuse();
   test_oom();
   if (failures)
   {
      fprintf(stderr, "rxml_write_test: %d failed\n", failures);
      return 1;
   }
   printf("rxml_write_test: all passed\n");
   return 0;
}

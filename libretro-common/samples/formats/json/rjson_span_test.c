/* Borrowed JSON spans must never modify or outlive caller input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rjson.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "span FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

typedef struct span_context
{
   const char *begin, *end;
   unsigned members;
   rjson_span_t array;
} span_context_t;

static bool visit(void *userdata, const rjson_span_t *name,
      const rjson_span_t *value)
{
   span_context_t *c = (span_context_t *)userdata;
   CHECK(name->data >= c->begin && name->data + name->length <= c->end);
   CHECK(value->data >= c->begin && value->data + value->length <= c->end);
   if (rjson_string_equal(name->data, name->length, "array"))
   {
      CHECK(value->type == RJSON_ARRAY && value->count == 3);
      c->array = *value;
   }
   c->members++;
   return true;
}

static bool abort_member(void *userdata, const rjson_span_t *name,
      const rjson_span_t *value)
{
   (void)userdata; (void)name; (void)value;
   return false;
}

static bool compare_child(void *unused, const rjson_span_t *name,
      const rjson_span_t *value)
{
   rjson_span_t strict, fast;
   (void)unused;
   CHECK(rjson_read_span(name->data, name->length, &strict, NULL, NULL));
   CHECK(rjson_read_span_validated(name->data, name->length, &fast, NULL, NULL));
   CHECK(strict.data == fast.data && strict.length == fast.length &&
         strict.type == fast.type && strict.count == fast.count);
   CHECK(rjson_read_span(value->data, value->length, &strict, NULL, NULL));
   CHECK(rjson_read_span_validated(value->data, value->length, &fast,
         compare_child, NULL));
   CHECK(strict.data == fast.data && strict.length == fast.length &&
         strict.type == fast.type && strict.count == fast.count);
   return true;
}

static void validated_spans(void)
{
   static const char *documents[] = {
      " {} ", "[]", "[[],{},[null,false,1.5e-2]]", "true", "false", "null",
      "-3.5e+7", "\"escaped \\\" quote\"",
      "{\"a\": [], \"b\":{\"c\": [true, false, null]}, \"a\":3}",
      "{\"a\":\"backslash\\\\\",\"b\":\"\\uD83D\\uDE00\"}"
   };
   size_t i;
   for (i = 0; i < sizeof(documents) / sizeof(documents[0]); ++i)
   {
      size_t length = strlen(documents[i]);
      char *input = (char *)malloc(length);
      rjson_span_t strict, fast;
      CHECK(input != NULL);
      if (!input)
         return;
      memcpy(input, documents[i], length);
      CHECK(rjson_read_span(input, length, &strict, NULL, NULL));
      CHECK(rjson_read_span_validated(input, length, &fast, compare_child, NULL));
      CHECK(strict.data == fast.data && strict.length == fast.length &&
            strict.type == fast.type && strict.count == fast.count);
      CHECK(rjson_read_span_validated(input, length, &fast, NULL, NULL));
      CHECK(strict.data == fast.data && strict.length == fast.length &&
            strict.type == fast.type && strict.count == fast.count);
      CHECK(!memcmp(input, documents[i], length));
      free(input);
   }
}

int rjson_span_test(void)
{
   static const char doc[] = " \n{\"array\":[1,{\"x\":true},[]],\"text\":\"a\\n\\uD83D\\uDE00\",\"nil\":null}";
   static const char quoted[] = "\"a\\n\\uD83D\\uDE00\"";
   static const char *bad[] = { "{", "[1,]", "{\"a\":}", "[01]", "[+1]",
      "[truex]", "{\"a\" 1}", "{\"a\":1,}", "\"\\uD800\"", "\"\\x\"",
      "\"a\n\"", "\"\xff\"", "\"abc", "\"a\\\"" };
   rjson_span_t span;
   span_context_t context;
   char output[32];
   char nested[128];
   size_t written = 0, i;
   validated_spans();
   context.begin = doc;
   context.end = doc + sizeof(doc) - 1;
   context.members = 0;
   CHECK(rjson_read_span(doc, sizeof(doc) - 1, &span, visit, &context));
   CHECK(span.data == doc + 2 && span.length == sizeof(doc) - 3);
   CHECK(span.type == RJSON_OBJECT && span.count == 3 && context.members == 3);
   CHECK(context.array.data[0] == '[' && context.array.data[context.array.length - 1] == ']');
   CHECK(!rjson_read_span(doc, sizeof(doc) - 1, &span, abort_member, NULL));
   CHECK(!rjson_read_span(NULL, 0, &span, NULL, NULL));
   for (i = 0; i < sizeof(doc) - 1; i++)
   {
      char *cut = (char *)malloc(i ? i : 1);
      CHECK(cut != NULL);
      if (!cut) return failures;
      memcpy(cut, doc, i);
      CHECK(!rjson_read_span(cut, i, &span, NULL, NULL));
      free(cut);
   }
   for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
   {
      size_t length = strlen(bad[i]);
      char *cut = (char *)malloc(length);
      CHECK(cut != NULL);
      if (!cut) return failures;
      memcpy(cut, bad[i], length);
      CHECK(!rjson_read_span(cut, length, &span, NULL, NULL));
      memset(output, 'X', sizeof(output));
      CHECK(!rjson_decode_string(cut, length, output, sizeof(output), NULL));
      CHECK(!memcmp(cut, bad[i], length));
      free(cut);
   }
   memset(output, 'X', sizeof(output));
   CHECK(rjson_decode_string(quoted, sizeof(quoted) - 1, output, sizeof(output), &written));
   CHECK(written == 6 && !memcmp(output, "a\n\xf0\x9f\x98\x80", 7));
   CHECK(output[written + 1] == 'X');
   CHECK(!rjson_decode_string(quoted, sizeof(quoted) - 1, output, 1, NULL));
   CHECK(rjson_string_equal(quoted, sizeof(quoted) - 1, "a\n\xf0\x9f\x98\x80"));
   CHECK(!rjson_string_equal("\"a\\u0000b\"", 10, "a"));
   CHECK(rjson_decode_string("\"\\b\\f\\r\\t\\/\\\\\\\"\"", 16, output, sizeof(output), &written));
   CHECK(written == 7 && !memcmp(output, "\b\f\r\t/\\\"", 8));
   CHECK(rjson_read_span("1.25e-3 trailing", 16, &span, NULL, NULL));
   CHECK(span.type == RJSON_NUMBER && span.length == 7);
   CHECK(rjson_read_span("{}tail", 6, &span, NULL, NULL) && span.length == 2);
   CHECK(rjson_read_span("[[],{}]", 7, &span, NULL, NULL) && span.count == 2);
   memset(nested, '[', 60);
   memset(nested + 60, ']', 60);
   CHECK(!rjson_read_span(nested, 120, &span, NULL, NULL));
   printf("borrowed span tests: %s\n", failures ? "FAILED" : "passed");
   return failures;
}

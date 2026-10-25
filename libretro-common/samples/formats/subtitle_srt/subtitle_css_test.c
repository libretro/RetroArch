/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_css_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../formats/subtitle/rsubtitle_vtt_internal.h"

static rsubtitle_vtt_css_token_t token;

static void expect(rsubtitle_vtt_css_t *css, enum rsubtitle_vtt_css_kind kind, const char *text)
{
   unsigned ticks = 0;
   int status = 0;
   while (!status && ticks++ < 300000) status = rsubtitle_vtt_css_step(css, &token);
   if (status != 1 || token.kind != kind || token.length != strlen(text)
         || memcmp(token.text, text, token.length < strlen(text) ? token.length : strlen(text)))
      fprintf(stderr, "CSS expected %u [%s], got status %d kind %u [%.*s]\n",
            (unsigned)kind, text, status, (unsigned)token.kind,
            (int)(token.length < 64 ? token.length : 64), token.text);
   assert(status == 1 && token.kind == kind);
   assert(token.length == strlen(text) && !memcmp(token.text, text, token.length));
   assert(token.text[token.length] == 0);
}

static void end(rsubtitle_vtt_css_t *css)
{
   unsigned ticks = 0;
   int status = 0;
   while (!status && ticks++ < 300000) status = rsubtitle_vtt_css_step(css, &token);
   assert(status == 2 && rsubtitle_vtt_css_step(css, &token) == 2);
}

static void begin(rsubtitle_vtt_css_t *css, const char *text)
{
   assert(rsubtitle_vtt_css_begin(css, text, strlen(text)));
   assert(!rsubtitle_vtt_css_begin(css, text, strlen(text)));
}

static void delimiter(rsubtitle_vtt_css_t *css, unsigned c)
{
   expect(css, RSUBTITLE_CSS_DELIM, "");
   assert(token.delimiter == c);
}

static void declarations(rsubtitle_vtt_css_t *css)
{
   begin(css, "/*comment*/::cue(.r\\65 d){color:rgb(1e2,50%,+.25);font-family:\"A; }\\\\B\"}");
   delimiter(css, ':'); delimiter(css, ':');
   expect(css, RSUBTITLE_CSS_FUNCTION, "cue"); delimiter(css, '.');
   expect(css, RSUBTITLE_CSS_IDENT, "red"); delimiter(css, ')'); delimiter(css, '{');
   expect(css, RSUBTITLE_CSS_IDENT, "color"); delimiter(css, ':');
   expect(css, RSUBTITLE_CSS_FUNCTION, "rgb");
   expect(css, RSUBTITLE_CSS_NUMBER, ""); assert(token.number == 100 && !token.flags);
   delimiter(css, ','); expect(css, RSUBTITLE_CSS_PERCENT, "");
   assert(token.number == 50 && token.flags == RSUBTITLE_CSS_INTEGER);
   delimiter(css, ','); expect(css, RSUBTITLE_CSS_NUMBER, "");
   assert(token.number == 0.25 && token.flags == RSUBTITLE_CSS_PLUS);
   delimiter(css, ')'); delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "font-family"); delimiter(css, ':');
   expect(css, RSUBTITLE_CSS_STRING, "A; }\\B"); delimiter(css, '}'); end(css);
}

static void identifiers(rsubtitle_vtt_css_t *css)
{
   static const char nul[] = {'a', 0, 'b'};
   begin(css, "#123 #cue @font-face --x -x co/**/lor cafe\\e9 ;\\1f642 ;\\0 ;\\d800 ;\\110000 ;" "\\");
   expect(css, RSUBTITLE_CSS_HASH, "123"); assert(!token.flags);
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_HASH, "cue");
   assert(token.flags == RSUBTITLE_CSS_ID);
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_AT, "font-face");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_IDENT, "--x");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_IDENT, "-x");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_IDENT, "co");
   expect(css, RSUBTITLE_CSS_IDENT, "lor"); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_IDENT, "cafe\303\251");
   delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "\360\237\231\202");
   delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "\357\277\275");
   delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "\357\277\275");
   delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "\357\277\275");
   delimiter(css, ';');
   expect(css, RSUBTITLE_CSS_IDENT, "\357\277\275"); end(css);
   assert(rsubtitle_vtt_css_begin(css, nul, sizeof(nul)));
   expect(css, RSUBTITLE_CSS_IDENT, "a\357\277\275b"); end(css);
   begin(css, "<!-- -->/**/");
   expect(css, RSUBTITLE_CSS_CDO, ""); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_CDC, ""); end(css);
}

static void strings(rsubtitle_vtt_css_t *css)
{
   begin(css, "\"a\\\r\nb\" 'x\\0000E9y' \"bad\nnext");
   expect(css, RSUBTITLE_CSS_STRING, "ab"); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_STRING, "x\303\251y"); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_BAD_STRING, ""); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_IDENT, "next"); end(css);
   begin(css, "\"unterminated\\");
   expect(css, RSUBTITLE_CSS_STRING, "unterminated"); end(css);
   begin(css, "/* unterminated"); end(css);
   begin(css, "\\\n"); delimiter(css, '\\'); expect(css, RSUBTITLE_CSS_SPACE, " "); end(css);
}

static void urls(rsubtitle_vtt_css_t *css)
{
   static const char after_bad[] = {'u','r','l','(','a','"','\\',')','}',')',' ',0};
   begin(css, "url(a\\)b) URL( \t'quoted') url(a\\29 b) url(a b) url(a\"}) after");
   expect(css, RSUBTITLE_CSS_URL, "a)b"); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_FUNCTION, "URL"); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_STRING, "quoted"); delimiter(css, ')');
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_URL, "a)b");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_BAD_URL, "");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_BAD_URL, "");
   expect(css, RSUBTITLE_CSS_SPACE, " "); expect(css, RSUBTITLE_CSS_IDENT, "after"); end(css);
   begin(css, "url(a\\\n)after");
   expect(css, RSUBTITLE_CSS_BAD_URL, ""); expect(css, RSUBTITLE_CSS_IDENT, "after"); end(css);
   assert(rsubtitle_vtt_css_begin(css, after_bad, sizeof(after_bad)));
   expect(css, RSUBTITLE_CSS_BAD_URL, ""); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_IDENT, "\357\277\275"); end(css);
   begin(css, "url(  a"); expect(css, RSUBTITLE_CSS_URL, "a"); end(css);
}

static void numbers(rsubtitle_vtt_css_t *css)
{
   static const struct
   {
      const char *input, *unit;
      double value;
      enum rsubtitle_vtt_css_kind kind;
      unsigned flags;
   } cases[] = {
      {"123", "", 123, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_INTEGER},
      {"-1.25e+2", "", -125, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_MINUS},
      {"2e2em", "em", 200, RSUBTITLE_CSS_DIMENSION, 0},
      {"-0.1%", "", -0.1, RSUBTITLE_CSS_PERCENT, RSUBTITLE_CSS_MINUS},
      {"-0", "", 0, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_INTEGER | RSUBTITLE_CSS_MINUS},
      {"+0", "", 0, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_INTEGER | RSUBTITLE_CSS_PLUS},
      {"10\\70 x", "px", 10, RSUBTITLE_CSS_DIMENSION, RSUBTITLE_CSS_INTEGER},
      {"1e99999999999", "", DBL_MAX, RSUBTITLE_CSS_NUMBER, 0},
      {"-1e999999", "", -DBL_MAX, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_MINUS},
      {"1e-999999", "", 0, RSUBTITLE_CSS_NUMBER, 0},
      {".00000000000000000000000001e26", "", 1, RSUBTITLE_CSS_NUMBER, 0},
      {"10000000000000000000000000", "", 1e25, RSUBTITLE_CSS_NUMBER, RSUBTITLE_CSS_INTEGER}
   };
   size_t i;
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      double difference, magnitude = cases[i].value;
      begin(css, cases[i].input); expect(css, cases[i].kind, cases[i].unit);
      assert(token.flags == cases[i].flags);
      difference = token.number - cases[i].value;
      if (difference < 0) difference = -difference;
      if (magnitude < 0) magnitude = -magnitude;
      assert(difference == 0 || difference / (magnitude ? magnitude : 1) < 1e-12);
      end(css);
   }
   begin(css, "12345678901234567e-330"); expect(css, RSUBTITLE_CSS_NUMBER, "");
   assert(token.number > 1e-315 && token.number < 2e-314); end(css);
   begin(css, "1e+ 1.");
   expect(css, RSUBTITLE_CSS_DIMENSION, "e"); assert(token.number == 1);
   delimiter(css, '+'); expect(css, RSUBTITLE_CSS_SPACE, " ");
   expect(css, RSUBTITLE_CSS_NUMBER, ""); assert(token.number == 1);
   delimiter(css, '.'); end(css);
}

static void limits(rsubtitle_vtt_css_t *css)
{
   char *input = (char*)malloc(65537);
   size_t i;
   assert(input);
   memset(input, 'x', 65537);
   assert(!rsubtitle_vtt_css_begin(css, input, 65537));
   assert(!rsubtitle_vtt_css_begin(css, NULL, 1));
   assert(rsubtitle_vtt_css_begin(css, input, 65536));
   {
      unsigned ticks = 0;
      int status = 0;
      while (!status && ticks++ < 65550) status = rsubtitle_vtt_css_step(css, &token);
      assert(status == 1 && token.kind == RSUBTITLE_CSS_IDENT && token.length == 65536);
   }
   end(css);
   memset(input, 0, 65536);
   assert(rsubtitle_vtt_css_begin(css, input, 65536));
   {
      unsigned ticks = 0;
      int status = 0;
      while (!status && ticks++ < 65550) status = rsubtitle_vtt_css_step(css, &token);
      assert(status == 1 && token.length == 65536u * 3);
      for (i = 0; i < token.length; i += 3) assert(!memcmp(token.text + i, "\357\277\275", 3));
   }
   end(css);
   assert(rsubtitle_vtt_css_begin(css, NULL, 0)); end(css);
   free(input);
}

static size_t components(rsubtitle_vtt_css_tree_t *tree, const char *input, size_t size)
{
   size_t root, pending = 0;
   int status = 0;
   assert(rsubtitle_vtt_css_tree_begin(tree, input, size, &root));
   assert(!rsubtitle_vtt_css_tree_count(tree) && !rsubtitle_vtt_css_tree_node(tree, root));
   while (!status && pending++ < size * 5 + 100) status = rsubtitle_vtt_css_tree_step(tree);
   assert(status == 1 && rsubtitle_vtt_css_tree_step(tree) == 1);
   return root;
}

static void component_links(void)
{
   static const char first[] = "f(a,[b,{c:d}])/*ignored*/ x";
   static const char second[] = "@font-face{font-family:\"second\\20 face\";src:url(a\\)b)}";
   const rsubtitle_vtt_css_node_t *node, *saved;
   rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
   size_t root, i, seen = 0;
   assert(tree && rsubtitle_vtt_css_tree_step(tree) == -1);
   assert(!rsubtitle_vtt_css_tree_count(NULL) && !rsubtitle_vtt_css_tree_node(NULL, 0));
   assert(!rsubtitle_vtt_css_tree_begin(NULL, NULL, 0, &root));
   assert(!rsubtitle_vtt_css_tree_begin(tree, NULL, 0, NULL));
   root = components(tree, first, sizeof(first) - 1);
   assert(!root && rsubtitle_vtt_css_tree_count(tree) == 13);
   node = rsubtitle_vtt_css_tree_node(tree, 0);
   assert(node && node->token.kind == RSUBTITLE_CSS_ROOT
         && node->parent == RSUBTITLE_VTT_NO_NODE && node->first_child == 1
         && node->next == RSUBTITLE_VTT_NO_NODE);
   saved = rsubtitle_vtt_css_tree_node(tree, 1);
   assert(saved && saved->token.kind == RSUBTITLE_CSS_FUNCTION
         && !strcmp(saved->token.text, "f") && saved->parent == 0
         && saved->first_child == 2 && saved->next == 11);
   node = rsubtitle_vtt_css_tree_node(tree, 4);
   assert(node && node->token.kind == RSUBTITLE_CSS_BLOCK && node->token.delimiter == '['
         && node->parent == 1 && node->first_child == 5);
   node = rsubtitle_vtt_css_tree_node(tree, 7);
   assert(node && node->token.kind == RSUBTITLE_CSS_BLOCK && node->token.delimiter == '{'
         && node->parent == 4 && node->first_child == 8);
   node = rsubtitle_vtt_css_tree_node(tree, 10);
   assert(node && !strcmp(node->token.text, "d") && node->parent == 7
         && node->first_child == RSUBTITLE_VTT_NO_NODE && node->next == RSUBTITLE_VTT_NO_NODE);
   root = components(tree, second, sizeof(second) - 1);
   assert(root == 13 && saved == rsubtitle_vtt_css_tree_node(tree, 1)
         && !strcmp(saved->token.text, "f"));
   for (i = root; i < rsubtitle_vtt_css_tree_count(tree); i++)
   {
      node = rsubtitle_vtt_css_tree_node(tree, i);
      if (node->token.kind == RSUBTITLE_CSS_STRING)
      {
         assert(!strcmp(node->token.text, "second face")); seen |= 1;
      }
      if (node->token.kind == RSUBTITLE_CSS_URL)
      {
         assert(!strcmp(node->token.text, "a)b")); seen |= 2;
      }
   }
   assert(seen == 3 && !rsubtitle_vtt_css_tree_node(tree, i));
   rsubtitle_vtt_css_tree_free(tree); rsubtitle_vtt_css_tree_free(NULL);
}

static void component_storage(void)
{
   rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
   const rsubtitle_vtt_css_node_t *saved, *node;
   char *input = (char*)malloc(65536);
   size_t i, size = 0, root;
   assert(tree && input);
   components(tree, "first", 5);
   saved = rsubtitle_vtt_css_tree_node(tree, 1);
   for (i = 0; i < 1000; i++) size += sprintf(input + size, "long_value%u ", (unsigned)i);
   components(tree, input, size);
   assert(saved == rsubtitle_vtt_css_tree_node(tree, 1) && !strcmp(saved->token.text, "first"));
   memset(input, 'x', 65536);
   root = components(tree, input, 65536);
   node = rsubtitle_vtt_css_tree_node(tree, root + 1);
   assert(node && node->token.length == 65536 && !memcmp(node->token.text, input, 65536)
         && !node->token.text[65536] && !strcmp(saved->token.text, "first"));
   root = components(tree, "([})", 4);
   assert(rsubtitle_vtt_css_tree_count(tree) == root + 5);
   node = rsubtitle_vtt_css_tree_node(tree, root + 3);
   assert(node && node->token.delimiter == '}' && node->parent == root + 2);
   node = rsubtitle_vtt_css_tree_node(tree, root + 4);
   assert(node && node->token.delimiter == ')' && node->parent == root + 2);
   free(input); rsubtitle_vtt_css_tree_free(tree);
}

static void component_node_limit(void)
{
   rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
   char *input = (char*)malloc(65536);
   unsigned block;
   assert(tree && input);
   memset(input, ';', 65536);
   for (block = 0; block < 4; block++)
   {
      size_t root, ticks = 0;
      int status = 0;
      assert(rsubtitle_vtt_css_tree_begin(tree, input, 65536, &root));
      while (!status && ticks++ < 131100) status = rsubtitle_vtt_css_tree_step(tree);
      assert(block < 3 ? status == 1 : status == -1);
   }
   assert(!rsubtitle_vtt_css_tree_count(tree) && !rsubtitle_vtt_css_tree_node(tree, 0));
   free(input); rsubtitle_vtt_css_tree_free(tree);
}

static void component_depth(void)
{
   char input[259];
   unsigned depth;
   for (depth = 128; depth <= 129; depth++)
   {
      rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
      size_t root, ticks = 0;
      int status = 0;
      assert(tree);
      memset(input, '(', depth); input[depth] = 'x';
      memset(input + depth + 1, ')', depth);
      assert(rsubtitle_vtt_css_tree_begin(tree, input, depth * 2 + 1, &root));
      while (!status && ticks++ < 2048) status = rsubtitle_vtt_css_tree_step(tree);
      assert(depth == 128 ? status == 1 : status == -1);
      if (status < 0)
      {
         assert(!rsubtitle_vtt_css_tree_count(tree) && !rsubtitle_vtt_css_tree_node(tree, 0));
         assert(!rsubtitle_vtt_css_tree_begin(tree, "", 0, &root));
      }
      rsubtitle_vtt_css_tree_free(tree);
   }
}

static void stylesheet_rules(void)
{
   const char *input = "<!-- @import url(\"a;b\"); ::cue(.a) { color: red } "
      "@media screen { ::cue { color: blue } } @tail token";
   rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
   rsubtitle_vtt_css_rules_t rules;
   rsubtitle_vtt_css_rule_t rule;
   size_t root, ticks = 0, media = RSUBTITLE_VTT_NO_NODE;
   unsigned count = 0;
   int status = 0;
   assert(tree && rsubtitle_vtt_css_tree_begin(tree, input, strlen(input), &root));
   assert(!rsubtitle_vtt_css_rules_begin(&rules, tree, root));
   while (!status && ticks++ < 4096) status = rsubtitle_vtt_css_tree_step(tree);
   assert(status == 1 && rsubtitle_vtt_css_rules_begin(&rules, tree, root));
   ticks = 0;
   while ((status = rsubtitle_vtt_css_rules_step(&rules, &rule)) < 2 && ticks++ < 128)
   {
      assert(status >= 0);
      if (!status) continue;
      assert(rule.first != RSUBTITLE_VTT_NO_NODE);
      assert(rule.at_rule == (count != 1));
      assert((rule.body != RSUBTITLE_VTT_NO_NODE) == (count == 1 || count == 2));
      if (count == 2) media = rule.body;
      count++;
   }
   assert(status == 2 && count == 4);
   assert(rsubtitle_vtt_css_rules_begin(&rules, tree, media));
   count = 0; ticks = 0;
   while ((status = rsubtitle_vtt_css_rules_step(&rules, &rule)) < 2 && ticks++ < 128)
   {
      assert(status >= 0);
      if (status == 1) { assert(!rule.at_rule); count++; }
   }
   assert(status == 2 && count == 1);
   input = "} ::cue {} --bad: {} ::cue(.b){} unfinished";
   assert(rsubtitle_vtt_css_tree_begin(tree, input, strlen(input), &root));
   ticks = 0; status = 0;
   while (!status && ticks++ < 4096) status = rsubtitle_vtt_css_tree_step(tree);
   assert(status == 1 && rsubtitle_vtt_css_rules_begin(&rules, tree, root));
   count = 0; ticks = 0;
   while ((status = rsubtitle_vtt_css_rules_step(&rules, &rule)) < 2 && ticks++ < 128)
   {
      assert(status >= 0);
      if (status != 1) continue;
      if (!count)
      {
         const rsubtitle_vtt_css_node_t *node = rsubtitle_vtt_css_tree_node(tree, rule.first);
         assert(node && node->token.kind == RSUBTITLE_CSS_DELIM && node->token.delimiter == '}');
      }
      count++;
   }
   assert(status == 2 && count == 2);
   assert(rsubtitle_vtt_css_rules_step(NULL, &rule) == -1);
   assert(rsubtitle_vtt_css_rules_step(&rules, NULL) == -1);
   rsubtitle_vtt_css_tree_free(tree);
}

static void stylesheet_declarations(void)
{
   static const char *names[] = { "color", "background", "empty", "zero", "nested",
      "notimportant", "repeat", "block", "--custom", "after", "tail" };
   static const char *first[] = { "red", "blue", NULL, NULL, "fn", "importantx",
      "!", NULL, "a", "ok", "green" };
   static const unsigned important[] = { 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1 };
   const char *input = "@ignored url(\"a;b\"); color : red ! /**/ ImPoRtAnT ; "
      "background: blue ; broken fn(x;y); empty: ; zero: !important; "
      "nested: fn(!important;{x}); notimportant: importantx; "
      "repeat: !important !important; block: {} !important; "
      "invalid: a {}; --custom: a {} b; @unknown { ignored: value } "
      "after: ok; tail: green !important";
   rsubtitle_vtt_css_tree_t *tree = rsubtitle_vtt_css_tree_new();
   rsubtitle_vtt_css_declarations_t decls;
   rsubtitle_vtt_css_declaration_t declaration;
   size_t root, ticks = 0, count = 0;
   int status = 0;
   assert(tree && rsubtitle_vtt_css_tree_begin(tree, input, strlen(input), &root));
   assert(!rsubtitle_vtt_css_declarations_begin(&decls, tree, root));
   while (!status && ticks++ < 8192) status = rsubtitle_vtt_css_tree_step(tree);
   assert(status == 1);
   assert(!rsubtitle_vtt_css_declarations_begin(NULL, tree, root));
   assert(!rsubtitle_vtt_css_declarations_begin(&decls, NULL, root));
   assert(!rsubtitle_vtt_css_declarations_begin(&decls, tree, root + 1));
   assert(rsubtitle_vtt_css_declarations_begin(&decls, tree, root));
   ticks = 0;
   while ((status = rsubtitle_vtt_css_declarations_step(&decls, &declaration)) < 2
         && ticks++ < rsubtitle_vtt_css_tree_count(tree) + 2)
   {
      const rsubtitle_vtt_css_node_t *node;
      assert(status >= 0);
      if (!status) continue;
      assert(count < sizeof(names) / sizeof(names[0]));
      node = rsubtitle_vtt_css_tree_node(tree, declaration.property);
      assert(node && node->token.length == strlen(names[count]));
      assert(!memcmp(node->token.text, names[count], node->token.length));
      assert(declaration.important == important[count]);
      if (first[count])
      {
         node = rsubtitle_vtt_css_tree_node(tree, declaration.first);
         assert(node);
         if (!strcmp(first[count], "!")) assert(node->token.delimiter == '!');
         else assert(node->token.length == strlen(first[count])
               && !memcmp(node->token.text, first[count], node->token.length));
      }
      else if (count == 7)
      {
         node = rsubtitle_vtt_css_tree_node(tree, declaration.first);
         assert(node && node->token.kind == RSUBTITLE_CSS_BLOCK);
         assert(declaration.first == declaration.last);
      }
      else assert(declaration.first == RSUBTITLE_VTT_NO_NODE
            && declaration.last == RSUBTITLE_VTT_NO_NODE);
      count++;
   }
   assert(status == 2 && count == sizeof(names) / sizeof(names[0]));
   assert(rsubtitle_vtt_css_declarations_step(NULL, &declaration) == -1);
   assert(rsubtitle_vtt_css_declarations_step(&decls, NULL) == -1);
   rsubtitle_vtt_css_tree_free(tree);
}

int main(void)
{
   rsubtitle_vtt_css_t *css = rsubtitle_vtt_css_new();
   assert(css && rsubtitle_vtt_css_step(css, &token) == -1);
   assert(rsubtitle_vtt_css_step(NULL, &token) == -1);
   assert(rsubtitle_vtt_css_step(css, NULL) == -1);
   assert(!rsubtitle_vtt_css_begin(NULL, "", 0));
   declarations(css); identifiers(css); strings(css); urls(css); numbers(css); limits(css);
   component_links(); component_storage(); component_depth();
   component_node_limit(); stylesheet_rules(); stylesheet_declarations();
   rsubtitle_vtt_css_free(css); rsubtitle_vtt_css_free(NULL);
   puts("PASS: incremental CSS tokens, comments, strings, escapes, numbers, URLs and limits");
   return 0;
}

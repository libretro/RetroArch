/* ryaml_test.c - the YAML reader over documents of the kinds it is used
 * for, with each node checked by hand: a configuration (nested block
 * mappings, plain, quoted and escaped scalars, empty flow collections, a
 * block sequence), block scalars, compact mappings on a sequence dash,
 * and a patch file in the shape RPCS3's patch.yml has, whose "load"
 * entries name sequences anchored under "Anchors:" by alias, from inside
 * flow sequences. Then anchors on scalars, mappings and sequences, an
 * alias of an alias, a name anchored twice, the documents that must be
 * refused (an alias with no anchor before it, an anchor with no name),
 * and where ryaml_location() places nodes.
 */

#include <stdio.h>
#include <string.h>
#include <formats/ryaml.h>

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("  FAIL: %s (line %d)\n", what, __LINE__); fails++; } } while (0)

static int val_is(const ryaml_t *y, int n, const char *want)
{
   size_t      len;
   const char *v = ryaml_val(y, n, &len);
   return v && len == strlen(want) && !memcmp(v, want, len);
}

static int key_is(const ryaml_t *y, int n, const char *want)
{
   size_t      len;
   const char *k = ryaml_key(y, n, &len);
   return k && len == strlen(want) && !memcmp(k, want, len);
}

static int nth(const ryaml_t *y, int n, int i)
{
   int c = ryaml_first_child(y, n);
   while (c >= 0 && i--)
      c = ryaml_next_sibling(y, c);
   return c;
}

static ryaml_t *parse(const char *doc)
{
   return ryaml_parse(doc, strlen(doc));
}

static void config(void)
{
   static const char doc[] =
      "Core:\n"
      "  PPU Decoder: Recompiler (LLVM)\n"
      "  PPU Threads: 2\n"
      "  SPU Block Size: Safe # a comment\n"
      "  Libraries Control: []\n"
      "  Empty: {}\n"
      "Video:\n"
      "  Renderer: \"Vulkan\"\n"
      "  Path: 'C:\\games\\it''s'\n"
      "  Escaped: \"a\\tb\\n\\\"c\\\"\"\n"
      "  Enabled: true\n"
      "List:\n"
      "  - one\n"
      "  - 0x10\n"
      "  -\n"
      "    nested: yes\n";
   ryaml_t *y = parse(doc);
   int      root, core, video, list, n;
   int      b = -1;
   uint32_t u = 0;

   printf("config\n");
   CHECK(y != NULL, "parses");
   if (!y)
      return;
   root  = ryaml_root(y);
   core  = ryaml_find_child(y, root, "Core");
   video = ryaml_find_child(y, root, "Video");
   list  = ryaml_find_child(y, root, "List");
   CHECK(ryaml_is_map(y, core) && ryaml_num_children(y, core) == 5, "Core is a mapping of five");
   CHECK(val_is(y, ryaml_find_child(y, core, "PPU Decoder"), "Recompiler (LLVM)"), "a plain scalar with spaces and brackets");
   CHECK(ryaml_val_uint(y, ryaml_find_child(y, core, "PPU Threads"), &u) && u == 2, "an integer");
   CHECK(val_is(y, ryaml_find_child(y, core, "SPU Block Size"), "Safe"), "a comment is not part of the value");
   n = ryaml_find_child(y, core, "Libraries Control");
   CHECK(ryaml_is_seq(y, n) && ryaml_num_children(y, n) == 0, "[] is an empty sequence");
   n = ryaml_find_child(y, core, "Empty");
   CHECK(ryaml_is_map(y, n) && ryaml_num_children(y, n) == 0, "{} is an empty mapping");
   CHECK(val_is(y, ryaml_find_child(y, video, "Renderer"), "Vulkan"), "a double-quoted scalar");
   CHECK(val_is(y, ryaml_find_child(y, video, "Path"), "C:\\games\\it's"), "a single-quoted scalar with ''");
   CHECK(val_is(y, ryaml_find_child(y, video, "Escaped"), "a\tb\n\"c\""), "escapes in a double-quoted scalar");
   CHECK(ryaml_val_bool(y, ryaml_find_child(y, video, "Enabled"), &b) && b == 1, "a boolean");
   CHECK(ryaml_is_seq(y, list) && ryaml_num_children(y, list) == 3, "a block sequence of three");
   CHECK(val_is(y, nth(y, list, 0), "one") && val_is(y, nth(y, list, 1), "0x10"), "its scalars");
   n = nth(y, list, 2);
   CHECK(ryaml_is_map(y, n) && val_is(y, ryaml_find_child(y, n, "nested"), "yes"), "an item on the lines below its dash");
   ryaml_free(y);
}

static void block_scalars(void)
{
   static const char doc[] =
      "lit: |\n"
      "  line one\n"
      "  line two\n"
      "fold: >-\n"
      "  folded\n"
      "  text\n"
      "seq:\n"
      "  - name: a\n"
      "    v: 1\n"
      "  - name: b\n"
      "    v: 2\n";
   ryaml_t *y = parse(doc);
   int      root, seq;

   printf("block scalars, compact mappings\n");
   CHECK(y != NULL, "parses");
   if (!y)
      return;
   root = ryaml_root(y);
   CHECK(val_is(y, ryaml_find_child(y, root, "lit"), "line one\nline two\n"), "a literal block scalar");
   CHECK(val_is(y, ryaml_find_child(y, root, "fold"), "folded text"), "a folded block scalar, stripped");
   seq = ryaml_find_child(y, root, "seq");
   CHECK(ryaml_num_children(y, seq) == 2, "two compact mappings");
   CHECK(val_is(y, ryaml_find_child(y, nth(y, seq, 1), "name"), "b")
         && val_is(y, ryaml_find_child(y, nth(y, seq, 1), "v"), "2"), "the second one's keys");
   ryaml_free(y);
}

/* RPCS3's patch.yml: a [ load, *anchor, modifier ] entry stands for the
 * anchored sequence of patches */
static void patch_file(void)
{
   static const char doc[] =
      "Version: 1.2\n"
      "\n"
      "Anchors:\n"
      "  common: &common\n"
      "    - [ be32, 0x00010000, 0x60000000 ]\n"
      "    - [ be32, 0x00010004, 0x4e800020 ]\n"
      "  fps: &fps [ [ bef32, 0x0002000, 60.0 ] ]\n"
      "\n"
      "PPU-0123456789abcdef0123456789abcdef01234567:\n"
      "  \"60 FPS\":\n"
      "    Games:\n"
      "      \"Some Game\":\n"
      "        BLUS00000: [ 01.00 ]\n"
      "    Author: someone\n"
      "    Patch:\n"
      "      - [ load, *common ]\n"
      "      - [ load, *fps, 0x10 ]\n"
      "      - [ be16, 0x00030000, 0x1234 ]\n";
   ryaml_t *y = parse(doc);
   int      root, hash, entry, patch, load, target, item, games, serial;

   printf("patch file with anchors\n");
   CHECK(y != NULL, "parses");
   if (!y)
      return;
   root  = ryaml_root(y);
   hash  = ryaml_find_child(y, root, "PPU-0123456789abcdef0123456789abcdef01234567");
   entry = ryaml_find_child(y, hash, "60 FPS");
   patch = ryaml_find_child(y, entry, "Patch");
   CHECK(ryaml_num_children(y, patch) == 3, "three patch entries");

   load   = nth(y, patch, 0);
   target = nth(y, load, 1);
   CHECK(val_is(y, nth(y, load, 0), "load"), "the entry's type");
   CHECK(ryaml_is_alias(y, target), "*common is an alias");
   CHECK(ryaml_is_seq(y, target) && ryaml_num_children(y, target) == 2, "holding the anchored sequence of two");
   item = nth(y, target, 1);
   CHECK(ryaml_is_seq(y, item) && val_is(y, nth(y, item, 0), "be32")
         && val_is(y, nth(y, item, 1), "0x00010004") && val_is(y, nth(y, item, 2), "0x4e800020"),
         "its second patch, through the alias");
   CHECK(nth(y, load, 2) < 0, "no modifier");

   load   = nth(y, patch, 1);
   target = nth(y, load, 1);
   CHECK(ryaml_is_seq(y, target) && ryaml_num_children(y, target) == 1, "*fps: a flow sequence anchored on one line");
   CHECK(val_is(y, nth(y, nth(y, target, 0), 2), "60.0"), "its value");
   CHECK(val_is(y, nth(y, load, 2), "0x10"), "the modifier after the alias");

   CHECK(!ryaml_is_alias(y, nth(y, patch, 2)) && val_is(y, nth(y, nth(y, patch, 2), 0), "be16"), "a plain entry");

   games  = ryaml_find_child(y, entry, "Games");
   serial = ryaml_find_child(y, ryaml_find_child(y, games, "Some Game"), "BLUS00000");
   CHECK(ryaml_is_seq(y, serial) && val_is(y, nth(y, serial, 0), "01.00"), "the serial's versions");
   ryaml_free(y);
}

static void anchors(void)
{
   static const char doc[] =
      "base: &b\n"
      "  x: 1\n"
      "  y: 2\n"
      "copy: *b\n"
      "name: &n hello\n"
      "again: *n\n"
      "list: &l\n"
      "  - a\n"
      "  - b\n"
      "seq:\n"
      "  - *l\n"
      "  - &item one\n"
      "  - *item\n"
      "chain: *b\n"
      "redef: &n second\n"
      "after: *n\n";
   ryaml_t *y = parse(doc);
   int      root, copy, seq, base;

   printf("anchors and aliases\n");
   CHECK(y != NULL, "parses");
   if (!y)
      return;
   root = ryaml_root(y);
   base = ryaml_find_child(y, root, "base");
   copy = ryaml_find_child(y, root, "copy");
   CHECK(ryaml_is_alias(y, copy) && !ryaml_is_alias(y, base), "an alias, and its anchor");
   CHECK(key_is(y, copy, "copy"), "the alias keeps its own key");
   CHECK(ryaml_is_map(y, copy) && ryaml_num_children(y, copy) == 2, "a mapping through the alias");
   CHECK(val_is(y, ryaml_find_child(y, copy, "y"), "2"), "find_child through the alias");
   CHECK(ryaml_has_children(y, copy) && !ryaml_has_val(y, copy), "children, no value");
   CHECK(val_is(y, ryaml_find_child(y, root, "again"), "hello"), "a scalar through the alias");
   seq = ryaml_find_child(y, root, "seq");
   CHECK(ryaml_num_children(y, seq) == 3, "three items");
   CHECK(ryaml_is_seq(y, nth(y, seq, 0)) && val_is(y, nth(y, nth(y, seq, 0), 1), "b"), "a sequence item that is an alias");
   CHECK(val_is(y, nth(y, seq, 1), "one") && val_is(y, nth(y, seq, 2), "one"), "an item anchored, then aliased");
   CHECK(ryaml_next_sibling(y, copy) == ryaml_find_child(y, root, "name"), "an alias keeps its place among its siblings");
   CHECK(ryaml_parent(y, copy) == root, "and its parent");
   CHECK(val_is(y, ryaml_find_child(y, root, "after"), "second"), "a name anchored again names the newer node");
   ryaml_free(y);
}

static void refused(void)
{
   static const char *bad[] = {
      "a: *nothing\n",
      "a: 1\nb: [ x, *missing ]\n",
      "a: & \n",
      "b: *later\nlater: &later 1\n",
      "a: &x 1\nb: *x trailing\n",
   };
   size_t i;
   printf("refused\n");
   for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
   {
      ryaml_t *y = parse(bad[i]);
      char     what[64];
      sprintf(what, "document %u refused", (unsigned)i);
      CHECK(y == NULL, what);
      ryaml_free(y);
   }
}

static void locations(void)
{
   static const char doc[] =
      "top:\n"
      "  key: value\n"
      "  list:\n"
      "    - [ a, *x ]\n"
      "x: &x 1\n";
   static const char doc2[] =
      "x: &x 1\n"
      "top:\n"
      "  key: value\n"
      "  list:\n"
      "    - [ a, *x ]\n";
   ryaml_t *y;
   size_t   line = 0, col = 0;
   int      top, list, flow;

   printf("locations\n");
   y = parse(doc);
   CHECK(y == NULL, "an alias ahead of its anchor is refused");
   ryaml_free(y);

   y = parse(doc2);
   CHECK(y != NULL, "parses");
   if (!y)
      return;
   top  = ryaml_find_child(y, ryaml_root(y), "top");
   CHECK(ryaml_location(y, top, &line, &col) && line == 2 && col == 1, "a top-level key");
   CHECK(ryaml_location(y, ryaml_find_child(y, top, "key"), &line, &col) && line == 3 && col == 3, "a nested key");
   list = ryaml_find_child(y, top, "list");
   flow = nth(y, list, 0);
   CHECK(ryaml_location(y, flow, &line, &col) && line == 5 && col == 5, "a sequence item, at its dash");
   CHECK(ryaml_location(y, nth(y, flow, 1), &line, &col) && line == 5 && col == 12, "a flow item");
   CHECK(!ryaml_location(y, RYAML_NONE, &line, &col), "no node, no location");
   ryaml_free(y);
}

int main(void)
{
   config();
   block_scalars();
   patch_file();
   anchors();
   refused();
   locations();
   printf(fails ? "ryaml: %d FAILED\n" : "ryaml: ok\n", fails);
   return fails ? 1 : 0;
}

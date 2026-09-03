/* Hashes the tree rxml builds for each document given, so two builds can
 * be compared against each other over a corpus.
 *
 * The point is regression, not correctness: the parser's output is a
 * linked tree with no textual form, so the way to check that a change to
 * it - an allocator, a fast path, an in-place termination - did not move
 * any byte is to hash names, text and attributes in document order and
 * diff the hashes from before and after.  Rejected documents print
 * REJECT, so a change in what is accepted shows up too.
 *
 *   cc -I libretro-common/include -o rxml_treehash \
 *      libretro-common/tools/formats/rxml_treehash.c \
 *      libretro-common/formats/xml/rxml.c \
 *      libretro-common/streams/file_stream.c \
 *      libretro-common/vfs/vfs_implementation.c \
 *      libretro-common/file/file_path.c \
 *      libretro-common/file/file_path_io.c \
 *      libretro-common/encodings/encoding_utf.c \
 *      libretro-common/string/stdstring.c \
 *      libretro-common/time/rtime.c \
 *      libretro-common/compat/compat_strl.c \
 *      libretro-common/compat/compat_posix_string.c
 *
 *   ./rxml_treehash corpus/[*].xml > before.txt
 *   ...change rxml...
 *   ./rxml_treehash corpus/[*].xml > after.txt
 *   diff before.txt after.txt
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <formats/rxml.h>
#include <formats/rxml_stream.h>

static uint64_t hash;

/* FNV-1a over the string, with a separator so ("ab","c") and ("a","bc")
 * do not collide, and a distinct mark for absent. */
static void mix(const char *s)
{
   if (!s)
   {
      hash = hash * 1099511628211ull + 7;
      return;
   }
   while (*s)
      hash = (hash ^ (unsigned char)*s++) * 1099511628211ull;
   hash = hash * 31 + 1;
}

static void walk(struct rxml_node *node)
{
   for (; node; node = node->next)
   {
      struct rxml_attrib_node *a;

      mix(node->name);
      mix(node->data);
      for (a = node->attrib; a; a = a->next)
      {
         mix(a->attrib);
         mix(a->value);
      }
      hash = hash * 31 + 3;
      walk(node->children);
      hash = hash * 31 + 5;
   }
}

int main(int argc, char **argv)
{
   int i;

   for (i = 1; i < argc; i++)
   {
      rxml_document_t *doc = rxml_load_document_filestream(argv[i]);

      if (!doc)
      {
         printf("%s REJECT\n", argv[i]);
         continue;
      }

      hash = 1469598103934665603ull;
      walk(rxml_root_node(doc));
      printf("%s %016llx\n", argv[i], (unsigned long long)hash);
      rxml_free_document(doc);
   }

   return 0;
}

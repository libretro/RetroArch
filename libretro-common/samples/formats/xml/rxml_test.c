/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml_test.c).
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

#include <formats/rxml.h>
#include <stdio.h>

static void print_siblings(struct rxml_node *node, unsigned level)
{
   fprintf(stderr, "\n%*sName: %s\n", level * 4, "", node->name);
   if (node->data)
      fprintf(stderr, "%*sData: %s\n", level * 4, "", node->data);

   for (const struct rxml_attrib_node *attrib =
         node->attrib; attrib; attrib = attrib->next)
      fprintf(stderr, "%*s  Attrib: %s = %s\n", level * 4, "",
            attrib->attrib, attrib->value);

   if (node->children)
      print_siblings(node->children, level + 1);

   if (node->next)
      print_siblings(node->next, level);
}

static void rxml_log_document(const char *path)
{
   rxml_document_t *doc = rxml_load_document(path);
   if (!doc)
   {
      fprintf(stderr, "rxml: Failed to load document: %s\n", path);
      return;
   }

   print_siblings(rxml_root_node(doc), 0);
   rxml_free_document(doc);
}

int main(int argc, char *argv[])
{
   if (argc != 2)
   {
      fprintf(stderr, "Usage: %s <path>\n", argv[0]);
      return 1;
   }

   rxml_log_document(argv[1]);
}

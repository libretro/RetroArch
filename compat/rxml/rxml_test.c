/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rxml.h"
#include <stdio.h>

static void print_siblings(struct rxml_node *node, unsigned level)
{
   fprintf(stderr, "\n%*sName: %s\n", level * 4, "", node->name);
   if (node->data)
      fprintf(stderr, "%*sData: %s\n", level * 4, "", node->data);

   for (const struct rxml_attrib_node *attrib = node->attrib; attrib; attrib = attrib->next)
      fprintf(stderr, "%*s  Attrib: %s = %s\n", level * 4, "", attrib->attrib, attrib->value);

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


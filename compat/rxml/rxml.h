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

#ifndef RXML_H__
#define RXML_H__

// Total NIH. Very trivial "XML" implementation for use in RetroArch.
// Error checking is minimal. Invalid documents may lead to very buggy behavior, but
// memory corruption should never happen.
//
// Only parts of standard that RetroArch cares about is supported.
// Nothing more, nothing less. "Clever" XML documents will probably break the implementation.
//
// Do *NOT* try to use this for anything else. You have been warned.

typedef struct rxml_document rxml_document_t;

struct rxml_attrib_node
{
   char *attrib;
   char *value;
   struct rxml_attrib_node *next;
};

struct rxml_node
{
   char *name;
   char *data;
   struct rxml_attrib_node *attrib;

   struct rxml_node *children;
   struct rxml_node *next;

   int type; // Dummy. Used by libxml2 compat. Is always set to 0, so XML_ELEMENT_NODE check goes through.
};

rxml_document_t *rxml_load_document(const char *path);
void rxml_free_document(rxml_document_t *doc);

struct rxml_node *rxml_root_node(rxml_document_t *doc);

// Drop const-correctness here to avoid warnings when used as libxml2 compat.
// xmlGetProp() returns xmlChar*, which is supposed to be passed to xmlFree().
char *rxml_node_attrib(struct rxml_node *node, const char *attrib);

#ifdef RXML_LIBXML2_COMPAT
// Compat for part of libxml2 that RetroArch uses.
#define LIBXML_TEST_VERSION ((void)0)
typedef char xmlChar; // It's really unsigned char, but it doesn't matter.
typedef struct rxml_node *xmlNodePtr;
typedef void *xmlParserCtxtPtr;
typedef rxml_document_t *xmlDocPtr;
#define XML_ELEMENT_NODE (0)
#define xmlNewParserCtxt() ((void*)-1)
#define xmlCtxtReadFile(ctx, path, ...) rxml_load_document(path)
#define xmlGetProp(node, prop) rxml_node_attrib(node, prop)
#define xmlFree(p) ((void)0)
#define xmlNodeGetContent(node) (node->data)
#define xmlDocGetRootElement(doc) rxml_root_node(doc)
#define xmlFreeDoc(doc) rxml_free_document(doc)
#define xmlFreeParserCtxt(ctx) ((void)0)
#endif

#endif


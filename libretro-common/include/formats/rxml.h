/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml.h).
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
#ifndef __LIBRETRO_SDK_FORMAT_RXML_H__
#define __LIBRETRO_SDK_FORMAT_RXML_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Total NIH. Very trivial "XML" implementation for use in RetroArch.
 * Error checking is minimal. Invalid documents may lead to very 
 * buggy behavior, but memory corruption should never happen.
 *
 * Only parts of standard that RetroArch cares about is supported.
 * Nothing more, nothing less. "Clever" XML documents will 
 * probably break the implementation.
 *
 * Do *NOT* try to use this for anything else. You have been warned.
 */

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

   /* Dummy. Used by libxml2 compat. 
    * Is always set to 0, so XML_ELEMENT_NODE check goes through. */
   int type; 
};

rxml_document_t *rxml_load_document(const char *path);
void rxml_free_document(rxml_document_t *doc);

struct rxml_node *rxml_root_node(rxml_document_t *doc);

/* Drop const-correctness here to avoid warnings 
 * when used as libxml2 compat.
 * xmlGetProp() returns xmlChar*, which is supposed 
 * to be passed to xmlFree(). */
char *rxml_node_attrib(struct rxml_node *node, const char *attrib);

#ifdef RXML_LIBXML2_COMPAT
/* Compat for part of libxml2 that RetroArch uses. */
#define LIBXML_TEST_VERSION ((void)0)
typedef char xmlChar; /* It's really unsigned char, but it doesn't matter. */
typedef struct rxml_node *xmlNodePtr;
typedef void *xmlParserCtxtPtr;
typedef rxml_document_t *xmlDocPtr;
#define XML_ELEMENT_NODE (0)
#define xmlNewParserCtxt() ((void*)-1)
#define xmlCtxtReadFile(ctx, path, a, b) rxml_load_document(path)
#define xmlGetProp(node, prop) rxml_node_attrib(node, prop)
#define xmlFree(p) ((void)0)
#define xmlNodeGetContent(node) (node->data)
#define xmlDocGetRootElement(doc) rxml_root_node(doc)
#define xmlFreeDoc(doc) rxml_free_document(doc)
#define xmlFreeParserCtxt(ctx) ((void)0)
#endif

RETRO_END_DECLS

#endif

